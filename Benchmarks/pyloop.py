import subprocess
import threading
import os
import signal
import sys
import shlex
import time

"""
    Threading-enabled class that will gracefully clean itself up upon request
    It loops a shell command from your CWD, so relative paths are OK
    It does start a new session, so it might not inherit all of your environment
    but it should have your user's environment (login state)
"""
class CommandLooper:
    def __init__(self):
        self.process = None
        self.running = False
        self.terminate = False
        self.lock = threading.Lock()

    def execute_command(self, command):
        self.lock.acquire()
        if self.running:
            # Wait for command to finish
            self.lock.release()
            return
        self.running = True
        self.lock.release()

        # Log what we're gonna do
        print(command)
        cmd = shlex.split(command)
        # Infinitely repeat the command until we terminate
        while not self.terminate:
            self.process = subprocess.Popen(cmd, start_new_session=True)
            self.process.wait()
            self.running = False

    def terminate_process(self):
        # Do not repeat the command, exit the while loop too
        self.terminate = True
        # If the process exists, destroy it and anything it spawns
        if self.process:
            try:
                pid = self.process.pid
                pgid = os.getpgid(pid)
                os.killpg(pgid, signal.SIGTERM)
            except ProcessLookupError:
                # The process already terminated
                pass
            except Exception as e:
                print(f"Failed to kill tree for process {pid} {e}")
                exit(2)

def main():
    executor = CommandLooper()
    # All arguments past "python3 <this_script_name.py>" are the thing to do
    command = " ".join(sys.argv[1:])
    # Ensure a command is actually given
    if command == "":
        print(f"python3 {sys.argv[0]} <COMMAND> <ARGS>")
        exit()
    # If backgrounded with nohup, there won't be STDIN, so log the PID to be nice
    if not sys.stdin.isatty():
        print(f"No stdin detected, my PID is {os.getpid()}")
    command_thread = threading.Thread(target=executor.execute_command, args=(command,))
    # Setup a signal handler for CTRL+C or external kill signals to still properly shut everything down
    def signal_handler(sig, frame):
        print(f"SIG HANDLER HIT WITH SIGNAL {sig}")
        executor.terminate_process()
        command_thread.join()
        exit(1)
    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGINT, signal_handler)
    print("Hit enter to terminate looping...")
    command_thread.start()
    try:
        input()
    except (OSError, EOFError):
        while True:
            time.sleep(86400)
    executor.terminate_process()
    command_thread.join()

if __name__ == '__main__':
    main()

