import time


class AsgiApplication:
    def __call__(self, arg):
        print(f"Called with argument: {arg}")
        time.sleep(10)
        print(f"Some with argument: {arg}")
        with open("abc.txt", "a") as f:
            f.write("hii\n")


def get_asgi_app():
    return AsgiApplication()


application = get_asgi_app()
