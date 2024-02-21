#include "output.h"

// Default Constructor
Output::Output(void) : std::ostream(&buffer),
                       buffer(std::cout, false),
                       defaultToCout(true),
                       detectedAsSudo(detectSudo()) {}
// Parameterized Constructor (with stream)
Output::Output(std::ostream& stream, bool timestamped = false) :
                       std::ostream(&buffer),
                       buffer(stream, timestamped),
                       defaultToCout(true),
                       detectedAsSudo(detectSudo()) {}
// Parameterized Constructor (with truthiness)
Output::Output(bool isCout, bool timestamped = false) :
                       std::ostream(&buffer),
                       buffer((isCout) ? std::cout : std::cerr, timestamped),
                       defaultToCout(isCout),
                       detectedAsSudo(detectSudo()) {}
Output::~Output(void) {
    // While current implementation has I/O access separated by processes,
    // it could become multithreaded at some point. Even though I expect
    // destructors and file selection to remain single-thread operations,
    // better to be a bit cautious now at negligible performance cost
    // than to forget this later and suffer
    std::lock_guard<std::mutex> lock(fileMutex);
    closeFile();
}
// Determine if running as sudo and get original user's UID/GID if so
bool Output::detectSudo(void) {
    const char* sudo_uid_str = getenv("SUDO_UID"),
              * sudo_gid_str = getenv("SUDO_GID");
    if (sudo_uid_str && sudo_gid_str) {
        sudo_uid = static_cast<uid_t>(std::stoi(sudo_uid_str));
        sudo_gid = static_cast<gid_t>(std::stoi(sudo_gid_str));
        return true;
    }
    return false;
}
// Operator overload here is ONLY to permit RHS operation, leave LHS alone
std::ostream& operator<<(std::ostream& os, Output& obj) {
    os << "Output[";
    if (obj.fname[0]) os << obj.fname << (obj.detectedAsSudo ? "; access mode: SUDO" : "; access mode: USER");
    else if (obj.is_cout()) os << "std::cout";
    else if (obj.is_cerr()) os << "std::cerr";
    else throw std::runtime_error("Lost track of filename, not std::cout or std::cerr");
    os << "]";
    return os;
}
// Properly close any open file handles and maintain internal state
void Output::closeFile(void) {
    if (fileStream.is_open() && is_custom()) fileStream.close();
    std::memset(fname, 0, NAME_BUFFER_SIZE);
}
// Open a given file, return if open was OK or not (optional pre-existence check)
// When previously identified as a sudoer, attempt to chown files to original user so they can interact with them properly in the future
bool Output::openFile(const char * openName, bool exists_ok = true) {
    std::lock_guard<std::mutex> lock(fileMutex);
    closeFile();
    if (!exists_ok && std::filesystem::exists(openName)) {
        std::cerr << "File '" << openName << "' already exists!" << std::endl;
        return false;
    }
    fileStream.open(openName, std::ios::out | std::ios::app);
    if (fileStream.is_open()) {
        if (detectedAsSudo && (chown(openName, sudo_uid, sudo_gid) == -1))
            std::cerr << "Failed to change ownership of file '" << openName <<"', will be owned by root" << std::endl;
        std::strncpy(fname, openName, NAME_BUFFER_SIZE);
        return true;
    }
    else {
        std::cerr << "Failed to open file '" << openName << "'" << std::endl;
        return false;
    }
}
// Determine where the current output is destined to arrive
bool Output::is_cout() { return buffer.getOutput() == &std::cout; }
bool Output::is_cerr() { return buffer.getOutput() == &std::cerr; }
bool Output::is_custom() { return (buffer.getOutput() != &std::cout) && (buffer.getOutput() != &std::cerr); }
// Revert from current output to the original default filestream
void Output::revert(void) {
    closeFile();
    if (defaultToCout) buffer.changeStream(std::cout);
    else buffer.changeStream(std::cerr);
}
// Change output destination to given filename
bool Output::redirect(const char * openName, bool exists_ok=true) {
    if (openFile(openName,exists_ok)) buffer.changeStream(fileStream);
    else {
        std::cerr << "Failed to redirect to file '" << openName << "'" << std::endl;
        revert();
        return false;
    }
    return true;
}
// Change output destination to given filesystem path
bool Output::redirect(std::filesystem::path fpath, bool exists_ok=true) { return redirect(fpath.string().c_str(), exists_ok); }

