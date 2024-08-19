#define PY_SSIZE_T_CLEAN
#include <Python.h>

int main(int argc, char *argv[]) {
    // Initialize the Python interpreter
    Py_Initialize();

    // Load the module object
    PyObject *pName = PyUnicode_DecodeFSDefault("mymodule");
    PyObject *pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule != NULL) {
        // Get the class from the module
        PyObject *pClass = PyObject_GetAttrString(pModule, "MyCallableClass");
        
        if (pClass && PyCallable_Check(pClass)) {
            // Create an instance of the class
            PyObject *pInstance = PyObject_CallObject(pClass, NULL);

            if (pInstance != NULL) {
                // Create a Python string for the argument
                PyObject *pValue = PyUnicode_FromString("hello");

                // Call the instance with the argument (this calls the __call__ method)
                PyObject *pResult = PyObject_CallObject(pInstance, PyTuple_Pack(1, pValue));
                
                // Check for and handle errors
                if (pResult != NULL) {
                    Py_DECREF(pResult);
                } else {
                    PyErr_Print();
                }
                
                // Clean up
                Py_DECREF(pValue);
                Py_DECREF(pInstance);
            } else {
                PyErr_Print();
            }

            Py_DECREF(pClass);
        } else {
            PyErr_Print();
        }

        Py_DECREF(pModule);
    } else {
        PyErr_Print();
    }

    // Finalize the Python interpreter
    Py_Finalize();

    return 0;
}
