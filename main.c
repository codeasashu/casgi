#include "asgi.h"

// Change the below function to accept a string parameters
void init_paths(const char *mypath) {

  int i;
  PyObject *pysys, *pysys_dict, *pypath;

  /* add cwd to pythonpath */
  pysys = PyImport_ImportModule("sys");
  if (!pysys) {
    PyErr_Print();
    exit(1);
  }
  pysys_dict = PyModule_GetDict(pysys);
  pypath = PyDict_GetItemString(pysys_dict, "path");
  if (!pypath) {
    PyErr_Print();
    exit(1);
  }
  if (PyList_Insert(pypath, 0, PyUnicode_FromString(".")) != 0) {
    PyErr_Print();
  }

  if (PyList_Insert(pypath, 0, PyUnicode_FromString(mypath)) != 0) {
    PyErr_Print();
  } else {
    printf("[INFO] adding %s to pythonpath.\n", mypath);
  }
}

int main(int argc, char *argv[]) {
  // Initialize the Python interpreter
  Py_Initialize();

  init_paths("/home/ashutosh/code/personal/casgi");

  // Load the module object
  PyObject *pName = PyUnicode_DecodeFSDefault("mymodule");
  PyObject *pModule = PyImport_Import(pName);
  Py_DECREF(pName);

  if (pModule != NULL) {
    // Get the class from the module
    PyObject *pClass = PyObject_GetAttrString(pModule, "AsgiApplication");

    if (pClass && PyCallable_Check(pClass)) {
      // Create an instance of the class
      PyObject *pInstance = PyObject_CallObject(pClass, NULL);

      if (pInstance != NULL) {
        // Create a Python string for the argument
        PyObject *pValue = PyUnicode_FromString("hello");

        // Call the instance with the argument (this calls the __call__ method)
        PyObject *pResult =
            PyObject_CallObject(pInstance, PyTuple_Pack(1, pValue));

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
