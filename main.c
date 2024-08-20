#include "asgi.h"

asgi_config *read_config(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    printf("Could not open config file: %s\n", filename);
    return NULL;
  }

  // Determine the file size
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  // Read the file contents into a buffer
  char *buffer = (char *)malloc(file_size + 1);
  if (!buffer) {
    printf("Memory allocation error\n");
    fclose(file);
    return NULL;
  }
  fread(buffer, 1, file_size, file);
  buffer[file_size] = '\0'; // Null-terminate the string

  fclose(file);

  // Parse the JSON
  cJSON *json = cJSON_Parse(buffer);
  if (!json) {
    printf("Error parsing JSON: %s\n", cJSON_GetErrorPtr());
    free(buffer);
    return NULL;
  }

  // Create and populate the asgi_config struct
  asgi_config *config = (asgi_config *)malloc(sizeof(asgi_config));
  if (!config) {
    printf("Memory allocation error\n");
    cJSON_Delete(json);
    free(buffer);
    return NULL;
  }

  // Extract "mypath" from the JSON
  cJSON *mypath_json = cJSON_GetObjectItem(json, "mypath");
  if (cJSON_IsString(mypath_json) && (mypath_json->valuestring != NULL)) {
    strncpy(config->app_path, mypath_json->valuestring,
            sizeof(config->app_path) - 1);
    config->app_path[sizeof(config->app_path) - 1] =
        '\0'; // Ensure null termination
  } else {
    printf("Invalid or missing 'mypath' in the configuration file.\n");
    free(config);
    config = NULL;
  }

  // Clean up
  cJSON_Delete(json);
  free(buffer);

  return config;
}

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

  struct casgi_server casgi;
  memset(&casgi, 0, sizeof(struct casgi_server));

  asgi_config *config = read_config("config.json");
  if (!config) {
    printf("Failed to load configuration.\n");
    exit(1);
  }
  casgi.config = config;
  // Initialize the Python interpreter
  Py_Initialize();

  init_paths(casgi.config->app_path);

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
