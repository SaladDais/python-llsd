/// Optional speedups module with optimized parse functions
/// Copyright Muttley WackyRaces 2022
/// MIT

#define Py_LIMITED_API 0x03070000
#define PY_SSIZE_T_CLEAN 1
#include <Python.h>

PyObject* create_parser_ret(PyObject* val, Py_ssize_t offset)
{
  return Py_BuildValue("OI", val, offset);
}

PyObject* set_error(const char* message)
{
  PyObject *mod_llsd_base = PyImport_ImportModule("llsd.base");
  assert (mod_llsd_base != NULL);
  PyObject* type_parse_error = PyObject_GetAttrString(mod_llsd_base, "LLSDParseError");
  assert (type_parse_error != NULL);

  PyErr_SetString(type_parse_error, message);
  return NULL;
}

PyObject* parse_delimited_string(PyObject* self, PyObject *args, PyObject *kwargs)
{
  PyObject *buffer;
  unsigned int index;
  unsigned char delim;
  if (!PyArg_ParseTuple(args, "OIB", &buffer, &index, &delim))
  {
    // exception is already set
    return NULL;
  }

  char *buffer_data;
  Py_ssize_t buffer_len;
  if (PyBytes_AsStringAndSize(buffer, &buffer_data, &buffer_len) < 0)
  {
    // exception is already set
    return NULL;
  }

  if (index >= buffer_len)
  {
    set_error("index exceeds buffer len");
    return NULL;
  }

  // Allocate scratch space in which to place the decoded string literal.
  // LLSD strings. The decoded string length can never be larger than
  // the encoded version under LLSD encoding rules. You might think that
  // you should use PyMem_RawAlloc, but the free for that seems to segfault
  // when using the limited API. Oh well.
  char *output_buffer;
  if ((output_buffer = malloc(buffer_len - index)) == NULL)
  {
    return NULL;
  }

  char ch = 0;
  char* parse_start = buffer_data + index;
  char* parse_end = buffer_data + buffer_len;
  char* parse_ptr = parse_start;
  char* output_ptr = output_buffer;
  PyObject* ret = NULL;

  // skip opening delimiter
  ++parse_ptr;

  // Note that python-llsd's tests require embedded nulls to be allowed in string literals,
  // so we can't switch on nullity to determine if we're at the end of the string. We need
  // an explicit length check every time we go through the loop.
  while (parse_ptr < parse_end)
  {
    switch (*parse_ptr)
    {
      /*
      case '\0': {
        set_error("Missing terminating quote when decoding 'string'");
        goto done_parsing;
      }
      */
      case '\'':
      case '\"':
      {
        if (*parse_ptr == delim)
        {
          ++parse_ptr;
          PyObject *str_val = PyUnicode_Decode(output_buffer, output_ptr - output_buffer, "utf8", NULL);
          // if decode fails, exception will be set and NULL returned
          if (str_val != NULL)
          {
            ret = create_parser_ret(str_val, ((char *) parse_ptr - parse_start));
            Py_DECREF(str_val);
          }
          goto done_parsing;
        }
        else
        {
          *(output_ptr++) = (*parse_ptr++);
        }
        break;
      }
      case '\\':
      {
        switch (*(++parse_ptr))
        {
          case 'a':  *(output_ptr++) = 0x7; parse_ptr++; continue;
          case 'b':  *(output_ptr++) = '\b'; parse_ptr++; continue;
          case 'f':  *(output_ptr++) = '\f'; parse_ptr++; continue;
          case 'n':  *(output_ptr++) = '\n'; parse_ptr++; continue;
          case 'r':  *(output_ptr++) = '\r'; parse_ptr++; continue;
          case 't':  *(output_ptr++) = '\t'; parse_ptr++; continue;
          case 'v':  *(output_ptr++) = 0xB; parse_ptr++; continue;

          case 'x':
          {
            ++parse_ptr;

            for (int hex_i = 0; hex_i < 2; hex_i ++)
            {
              switch (*parse_ptr)
              {
                case '\0': {
                  set_error("Unterminated hex escape sequence when decoding 'string'");
                  goto done_parsing;
                }
                default: {
                  set_error("Unexpected character in hex escape sequence when decoding 'string'");
                  goto done_parsing;
                }
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                  ch = (ch << 4) + (*parse_ptr - '0');
                  break;

                case 'a':
                case 'b':
                case 'c':
                case 'd':
                case 'e':
                case 'f':
                  ch = (ch << 4) + 10 + (*parse_ptr - 'a');
                  break;

                case 'A':
                case 'B':
                case 'C':
                case 'D':
                case 'E':
                case 'F':
                  ch = (ch << 4) + 10 + (*parse_ptr - 'A');
                  break;
              }

              ++parse_ptr;
            }

            // write the ORed together char
            *(output_ptr++) = ch;
            break;
          }

          /*
          case '\0': {
            set_error("Unterminated escape sequence when decoding 'string'");
            goto done_parsing;
          }
          */
          default:
            // if we don't know what an escape code means, we have to include it verbatim.
            *(output_ptr++) = *(parse_ptr++);
        }
        break;
      }

      default:
      {
        *(output_ptr++) = *(parse_ptr++);
        break;
      }
    }
  }
  set_error("Missing terminating quote when decoding 'string'");
  done_parsing:
  free(output_buffer);
  return ret;
}

static PyMethodDef llsdMethods[] = {
  {"parse_delimited_string", (PyCFunction) parse_delimited_string, METH_VARARGS, NULL},
  {NULL, NULL, 0, NULL}       /* Sentinel */
};


static int module_traverse(PyObject *m, visitproc visit, void *arg);
static int module_clear(PyObject *m);
static void module_free(void *m);

static struct PyModuleDef moduledef = {
  PyModuleDef_HEAD_INIT,
  "llsd._speedups",
  0,                    /* m_doc */
  0,                    /* m_size */
  llsdMethods,          /* m_methods */
};

PyMODINIT_FUNC PyInit__speedups(void)
{
  PyObject* module;

  // This function is not supported in PyPy.
  if ((module = PyState_FindModule(&moduledef)) != NULL)
  {
    Py_INCREF(module);
    return module;
  }

  module = PyModule_Create(&moduledef);
  if (module == NULL)
  {
    return NULL;
  }

  return module;
}
