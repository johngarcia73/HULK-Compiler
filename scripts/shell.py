import subprocess

class Shell:
    def __rshift__(self, command:str)->str:
        # check_output raises CalledProcessError if the command fails (exit code != 0)
        try:
            ans =  subprocess.check_output(command, shell=True, text=True).strip()
            print(ans)
            return ans
        except subprocess.CalledProcessError as e:
            # You can decide to return the error or re-raise a custom one
            return f"Error: Command failed with code {e.returncode}"
