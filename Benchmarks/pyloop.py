import subprocess
import threading
import os
import signal
import sys
import shlex

class CommandLooper:
    def __init__(self):
        self.process = None
        self.running = False
        self.lock = threading.Lock()

    def execute_command(self, command):
        self.lock.acquire()
        if self.running:
            # Wait for command to finish
            self.lock.release()
            return
        self.running = True
        self.lock.release()

        print(command)
        cmd = shlex.split(command)
        #cmd = command
        print(cmd)
        while True:
            self.process = subprocess.Popen(cmd)
            self.process.wait()
            self.running = False

    def terminate_process(self):
        if self.process:
            try:
                pid = self.process.pid
                pgid = os.getpgid(pid)
                os.killpg(pgid, signal.SIGTERM)
            except ProcessLookupError:
                # The process already terminated
                pass
            except:
                print(f"Failed to kill tree for process {pid}")
                raise

def usage():
    print(f"python3 {sys.argv[0]} <COMMAND> <ARGS>")
    exit()

def main():
    executor = CommandLooper()
    command = " ".join(sys.argv[1:])
    # Ensure a command is actually given
    if command == "":
        usage()
    command_thread = threading.Thread(target=executor.execute_command, args=(command,))
    print("Hit enter to terminate looping...")
    command_thread.start()
    input()
    executor.terminate_process()

if __name__ == '__main__':
    main()

