#include "pdu_tools.h"
#include "snmp_hosts_and_oids.h" // Prevent multiple definition
#include "snmp.c" // This should be separately built/linked, but I have had it with CMake not ordering this correctly

void cache_pdus(void) {
    // No caching if we aren't going to query PDUs
    if (!args.pdu) return;

    if (args.debug >= DebugVerbose)
        args.error_log << "Begin caching PDUs" << std::endl;

    // Cache PDUs over SNMP
    int candidate_index = 0;
    for (int i = 0; i < N_PDU_ENDPOINTS; i++) {
        // Prepare candidate
        std::unique_ptr<pdu_cache> candidate = std::make_unique<pdu_cache>();
        candidate->index = candidate_index;
        candidate->sockfd = openSNMP(pdu_endpoints[i], &candidate->addr);
        if (candidate->sockfd == -1) {
            args.error_log << "Failed to open connection to PDU_ENDPOINT " << i << " at " << pdu_endpoints[i] << std::endl;
            continue;
        }
        candidate_index += 1; // Only increment when host properly registers
        candidate->host = pdu_endpoints[i];

        // Set up oid_cache mappings
        for (int j = 0; j < N_PDU_OIDS; j++) {
            candidate->oid_cache.values.push_back(-1);
            candidate->oid_cache.oid_names.push_back(pdu_oids[j]);
            candidate->oid_cache.field_names.push_back(pdu_oid_elements[j]);
        }
        // Set up SNMP messages to retrieve OIDS
        for (int j = 0; j < N_PDU_OIDS; j++) {
            byte *msg = createGetRequestMessage(SNMP_V1, (byte*)"public", 6, pdu_oids, N_PDU_OIDS);
            if (decodeLen(&msg[1]) <= SNMP_SendMax) {
                candidate->requestMessages.push_back(msg);
            }
            else {
                // Have to split into multiple messages
                // TODO: The current method works in practice but is not technically correct.
                // If the split message's fragments are still too long, it should continue splitting until they are short enough
                size_t msgLen = decodeLen(&msg[1]);
                free(msg);
                // Round values up
                int fragments = (msgLen + SNMP_SendMax - 1) / SNMP_SendMax,
                    oids_per_fragment = (N_PDU_OIDS + fragments - 1) / fragments;
                size_t used = 0;
                while (used < N_PDU_OIDS) {
                    int group = oids_per_fragment;
                    if (group + used >= N_PDU_OIDS) group = N_PDU_OIDS - used;
                    byte *msg_fragment = createGetRequestMessage(SNMP_V1, (byte*)"public", 6, &pdu_oids[used], group);
                    candidate->requestMessages.push_back(msg_fragment);
                    used += group;
                }
            }
        }
        // No temperature to cache, no initial data retrieval
        known_pdus.emplace_back(std::move(candidate));
        pdus_to_satisfy++;
    }

    // Disable if none available
    if (pdus_to_satisfy == 0) {
        args.error_log << "No PDUs cached, disabling future polling" << std::endl;
        args.pdu = 0;
    }
    else if (args.debug >= DebugMinimal)
        args.error_log << "Tracking " << pdus_to_satisfy << " PDUs" << std::endl;
}

int parseInteger(byte *string) {
    size_t len = decodeLen(&string[1]),
           len_enc = getEncodedLenLen(len),
           len_limit = 1+len+len_enc;
    int r = 0;
    for (size_t cursor = 1 + len_enc; cursor < len_limit; cursor++) {
        r << 8;
        r += (int)string[cursor];
    }
    return r;
}

int update_pdus(void) {
    if (args.debug >= DebugVerbose) args.error_log << "Update PDUs" << std::endl;
    for (std::vector<std::unique_ptr<pdu_cache>>::iterator i = known_pdus.begin(); i != known_pdus.end(); i++) {
        pdu_cache* j = i->get();
        // Retrieve and update info
        for (size_t k = 0; k < j->requestMessages.size(); k++) {
            int r = sendSNMP(j->sockfd, j->addr, j->requestMessages.at(k));
            if (r != 0) {
                // r < 0 is a send error (close host, cleanup, reconnect, send again, etc)
                // r > 0 is a partial send (should send again)
                // TODO: After defining a way to properly handle the above issues, change this to a WHILE loop with some limited cap on attempts before disabling the request for this pdu_cache entry
                args.error_log << "Failed to send SNMP for PDU " << j->index << std::endl;
                continue;
            }
            r = recvSNMP(j->sockfd, j->addr, j->lastResponse, SNMP_ResponseMax);
            if (r != 0) {
                // r < 0 is a recv error (close host, cleanup, reconnect, receive again, etc)
                // r > 0 is a peer close (attempt to reconnect)
                // TODO: After defining a way to properly handle the above issues, change this to a WHILE loop with a limited cap on attempts before disabling the request for this pdu_cache entry
                args.error_log << "Failed to receive SNMP for PDU " << j->index << std::endl;
                continue;
            }

            // Parse the response sequence of j->lastResponse
            byte *response = &j->lastResponse[0],
                  type = response[0];
            if (type != SNMP_Sequence) {
                args.error_log << "Received non-SNMP_Sequence (" << SNMP_Sequence << ") type: " << type << ", skipping updates for PDU " << j->index << std::endl;
                continue;
            }
            size_t seqLen = decodeLen(&response[1]),
                   len_enc = getEncodedLenLen(seqLen),
                   cursor = 1 + len_enc,
                   seqOffset = 0,
                   len;
            // Jump over components until reaching response portion
            while (seqOffset < seqLen) {
                type = response[cursor];
                len = decodeLen(&response[cursor+1]);
                len_enc = getEncodedLenLen(len);
                if (type == SNMP_GetRsp) break;
                seqOffset += 1 + len_enc + len;
                cursor += 1 + len_enc + len;
            }
            // Begin processing the Get Response portion
            cursor += 1 + len_enc;
            seqOffset += 1 + len_enc;
            // Response ID comment (skip)
            len = decodeLen(&response[cursor+1]);
            len_enc = getEncodedLenLen(len);
            cursor += 1 + len_enc + len;
            seqOffset += 1 + len_enc + len;
            // Error Component (halt processing this entry if erroneous)
            if (response[cursor+2] != 0x00) {
                args.error_log << "SNMP Response indicated error " << response[cursor+2] << ", skipping updates for PDU " << j->index << std::endl;
                continue;
            }
            cursor += 3;
            seqOffset += 3;
            // Error Index Component (skip)
            len = decodeLen(&response[cursor+1]);
            len_enc = getEncodedLenLen(len);
            cursor += 1 + len_enc + len;
            seqOffset += 1 + len_enc + len;
            // Now at the varbind list where all requests are answered
            while (seqOffset < seqLen) {
                size_t listLen = decodeLen(&response[cursor+1]),
                       listOffset = 0;
                len_enc = getEncodedLenLen(listLen);
                cursor += 1 + len_enc;
                seqOffset += 1 + len_enc;
                while (listOffset < listLen) {
                    // Varbind metadata
                    len = decodeLen(&response[cursor+1]);
                    len_enc = getEncodedLenLen(len);
                    cursor += 1 + len_enc;
                    seqOffset += 1 + len_enc;
                    listOffset += 1 + len_enc;
                    // OID component
                    std::string oid = "";
                    len = decodeLen(&response[cursor+1]);
                    len_enc = getEncodedLenLen(len);
                    cursor += 1 + len_enc;
                    seqOffset += 1 + len_enc;
                    listOffset += 1 + len_enc;
                    bool skip = false;
                    size_t c = 0;
                    for (size_t x = 0; x < len; x++) {
                        if (skip) {
                            if (! (response[cursor+x] & 0x80))
                                skip = false;
                            continue;
                        }
                        if (response[cursor+x] & 0x80)
                            skip = true;
                        if (response[cursor+x] == 0x2b)
                            oid += "1.3";
                        else {
                            int oid_value = decodeOID(&response[cursor+x]);
                            oid += "."+std::to_string(oid_value);
                        }
                    }
                    cursor += len;
                    seqOffset += len;
                    listOffset += len;
                    // Value Component
                    len = decodeLen(&response[cursor+1]);
                    len_enc = getEncodedLenLen(len);
                    // OID may not be tracked, if so, skip it
                    int y = 0;
                    for (std::vector<std::string>::iterator x = j->oid_cache.oid_names.begin(); x != j->oid_cache.oid_names.end(); x++) {
                        if (x->compare(oid) == 0) {
                            j->oid_cache.values[y] = parseInteger(&response[cursor]);
                            break;
                        }
                        y++;
                    }
                    cursor += 1 + len_enc + len;
                    seqOffset += 1 + len_enc + len;
                    listOffset += 1 + len_enc + len;
                }
            }
        }
        // Output
        if (args.debug >= DebugVerbose || update) {
            switch (args.format) {
                case OutputCSV:
                    for (int k = 0; k < N_PDU_OIDS; k++)
                        args.log << "," << j->oid_cache.values[k];
                    break;
                case OutputHuman:
                    for (int k = 0; k < N_PDU_OIDS; k++)
                        args.log << j->oid_cache.field_names[k] << ": " << j->oid_cache.values[k] << std::endl;
                    break;
                case OutputJSON:
                    for (int k = 0; k < N_PDU_OIDS; k++)
                        args.log << "\t\"pdu-" << j->index << "-" << j->oid_cache.field_names[k] << "\": " << j->oid_cache.values[k] << "," << std::endl;
                    break;
            }
        }
    }
    // No temperatures == always fully satisfied
    return pdus_to_satisfy;
}


// Definition of external variables for PDU tools
std::vector<std::unique_ptr<pdu_cache>> known_pdus;
int pdus_to_satisfy = 0;

