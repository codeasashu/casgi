# import sys


class AsgiApplication:
    callback = None

    def __call__(self, arg, fputs):
        print(f"Called with argument: {arg}")
        # fputs("Real Python!", "write.txt")
        self.callback = fputs
        ret = self.execute("ANSWER")
        print(f"Some with argument: {arg}. ret={ret}")

    def execute(self, command, *args):
        try:
            self.send_command(command, *args)
            # return self.get_result()
        except IOError as e:
            if e.errno == 32:
                # Broken Pipe * let us go
                raise Exception("Received SIGPIPE")
            else:
                raise

    def send_command(self, command, *args):
        """Send a command to Asterisk"""
        command = command.strip()
        command = "%s %s" % (command, " ".join(map(str, args)))
        command = command.strip()
        if command[-1] != "\n":
            command += "\n"
        # sys.stderr.write("    COMMAND: %s" % command)
        ret = self.callback(command)  # noqa
        print(f"return after cmd: {ret}")


def get_asgi_app():
    return AsgiApplication()


application = get_asgi_app()
