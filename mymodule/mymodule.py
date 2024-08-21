# mymodule.py
class AsgiApplication:
    def __call__(self, arg):
        print(f"Called with argument: {arg}")


def get_asgi_app():
    return AsgiApplication()


application = get_asgi_app()
