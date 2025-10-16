// ParityQC © 2025. See LICENSE.txt in /src/isv/parityqc/ for details.

/** @file
 * @brief The QDMI Device implementation of the ParityOS Device at Leibniz
 * Supercomputing Centre.
 * @details TODO: add description!
 */

#include "qdmi/constants.h"
#include <Python.h>
#include <assert.h>
#include <cstddef>
#include <cstdlib>
#include <parityos_qdmi/device.h>
#include <string>

//===----------------------------------------------------------------------===//
// Private code
//===----------------------------------------------------------------------===//

namespace {
/// Environment variable containing absolute path to the folder containing all relevant python scripts.
#define SCRIPT_PATH "PARITYQC_SCRIPT_PATH"
/// Environment variable containing basename of the python script without extension (e.g. `my_script`, not `my_script.py`).
#define SCRIPT_NAME "PARITYQC_PARITYOS_WRAPPER_SCRIPT_NAME"

/// FIXME: do we want that?
/// NOTE: For ease of discoverability we prefix private functions with `local_`.

enum class SESSION_STATUS {
  /// Session starts in allocated state.
  ALLOCATED,
  /// A session moves to the initialized state once
  /// ParityOS_QDMI_device_session_init was called.
  INITIALIZED,
};

/// FIXME: This was in the template. Document what the device status even means
/// for us! After that check if this implementation really satisfies our
/// specification.
QDMI_Device_Status *local_get_device_status() {
  static QDMI_Device_Status device_status = QDMI_DEVICE_STATUS_OFFLINE;
  return &device_status;
}

QDMI_Device_Status local_read_device_status() {
  return *local_get_device_status();
}

void local_set_device_status(QDMI_Device_Status status) {
  *local_get_device_status() = status;
}

/// FIXME: check if this is true.
/// FIXME: setting the initial value is a bit indirect. We should put all the
/// python stuff into one object for better resource management.
///
/// It can happen that our device is called from a python process. E.g. if the
/// driver is implemented in python. Certain code paths depend on that.
bool is_from_python() {
  static bool is_initialized = Py_IsInitialized() != 0;
  return is_initialized;
}

/** @brief Checks if there is a python related error.
 *
 * This macro checks whether the `value` is `nullptr` or `Py_None`. If it is, it
 * returns a `QDMI_ERROR_FATAL`.
 *
 * It also auto-detects whether it needs to release the gil.
 */
#define CHECK_PYTHON_ERROR(value)                                              \
  {                                                                            \
    if (value == Py_None || value == nullptr) {                                \
                                                                               \
      PyErr_Print();                                                           \
      if (!is_from_python()) {                                                 \
        PyGILState_Release(gstate);                                            \
      }                                                                        \
      return QDMI_ERROR_FATAL;                                                 \
    }                                                                          \
  }

/// Refers to the parityos wrapper script.
PyObject **get_parityos_module() {
  static PyObject *python_module = nullptr;
  return &python_module;
}

/// FIXME: documentation
int initialize_python() {
  //const auto script_path = std::getenv(SCRIPT_PATH);
  const auto script_path = "/workspaces/MQSS-QDMI-Devices-Suite/src/isv/parityqc"; // FIXME: why is env variable not working?
  const auto script_name = std::getenv(SCRIPT_NAME);

  // FIXME: ERROR if envs not available. What about logging?
  assert(script_path && "Missing script path");
  assert(script_name && "Missing script name");

  // Arbitrary and irrelevant initial value for `gstate` to suppress `-Wmaybe-uninitialized`.
  PyGILState_STATE gstate = PyGILState_LOCKED;
  if (!is_from_python()) {
    Py_Initialize();
    PyThreadState *_save = PyEval_SaveThread();
    gstate = PyGILState_Ensure();
  }

  PyObject *sys = PyImport_ImportModule("sys");
  PyObject *sys_path = PyObject_GetAttrString(sys, "path");

  PyList_Append(sys_path, PyUnicode_FromString(script_path));

  PyObject *pName = PyUnicode_DecodeFSDefault(script_name);
  CHECK_PYTHON_ERROR(pName);

  auto module = PyImport_Import(pName);
  *get_parityos_module() = module;
  CHECK_PYTHON_ERROR(*get_parityos_module());

  Py_XDECREF(pName);

  if (!is_from_python())
    PyGILState_Release(gstate);

  return QDMI_SUCCESS;
}

} // namespace

//===----------------------------------------------------------------------===//
// Macros
//===----------------------------------------------------------------------===//

/// Copy a scalar value and report its size:
/// - copy `prop_value` into `value` (conditionally)
/// - `size_ret` reports the length of the copied value (conditionally)
#define ADD_SINGLE_VALUE_PROPERTY(prop_name, prop_type, prop_value, prop,      \
                                  size, value, size_ret)                       \
  {                                                                            \
    if ((prop) == (prop_name)) {                                               \
      if ((value) != nullptr) {                                                \
        if ((size) < sizeof(prop_type)) {                                      \
          return QDMI_ERROR_INVALIDARGUMENT;                                   \
        }                                                                      \
        *static_cast<prop_type *>(value) = prop_value;                         \
      }                                                                        \
      if ((size_ret) != nullptr) {                                             \
        *size_ret = sizeof(prop_type);                                         \
      }                                                                        \
      return QDMI_SUCCESS;                                                     \
    }                                                                          \
  }

/// Copy a string and report its length including null-terminator:
/// - copy string `prop_value` into `value` (conditionally)
/// - `size_ret` reports the length of the copied value (conditionally)
#define ADD_STRING_PROPERTY(prop_name, prop_value, prop, size, value,          \
                            size_ret)                                          \
  {                                                                            \
    if ((prop) == (prop_name)) {                                               \
      if ((value) != nullptr) {                                                \
        if ((size) < strlen(prop_value) + 1) {                                 \
          return QDMI_ERROR_INVALIDARGUMENT;                                   \
        }                                                                      \
        strncpy(static_cast<char *>(value), prop_value, size);                 \
        static_cast<char *>(value)[size - 1] = '\0';                           \
      }                                                                        \
      if ((size_ret) != nullptr) {                                             \
        *size_ret = strlen(prop_value) + 1;                                    \
      }                                                                        \
      return QDMI_SUCCESS;                                                     \
    }                                                                          \
  }

/// If `value` is not a qdmi success return it.
#define CHECK_QDMI_ERROR(value)                                                \
  {                                                                            \
    if (value != QDMI_SUCCESS) {                                               \
      return value;                                                            \
    }                                                                          \
  }

//===----------------------------------------------------------------------===//
// QDMI opaque types implementation
//===----------------------------------------------------------------------===//

struct ParityOS_QDMI_Device_Session_impl_d {
  enum SESSION_STATUS status = SESSION_STATUS::ALLOCATED;
  /// For authentication with parityapi, e.g. `https://api.parityqc.com/`.
  std::string base_url = "";
  /// For authentication with parityapi
  std::string username = "";
  /// Let it be hardcoded for now.
  std::string api_version = "v3";
};

/// TODO: We might want to refactor this object once we have a working
/// prototype.
struct ParityOS_QDMI_Device_Job_impl_d {
  // Const fields:

  /// NOTE: Session *not* managed by this object!
  const ParityOS_QDMI_Device_Session session = nullptr;
  /// TODO: In case we need an id it probably should be universally unique (e.g.
  /// uuid).
  const int id = -1;

  // Mutable fields:

  QDMI_Job_Status status = QDMI_JOB_STATUS_CREATED;
  /// TODO: Here we probably need a *custom* (json) format. We probably cannot
  /// validate at this point if the format is consistent with program. I think
  /// it is OK that the service just returns an error if the program is not as
  /// the format claims.
  QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_CUSTOM1;
  /// TODO: This will probaly contain parityqc problem representation in json
  /// format
  void *program = nullptr;

  /// TODO: add more fields as needed.

  ParityOS_QDMI_Device_Job_impl_d(const ParityOS_QDMI_Device_Session session_)
      : session(session_), id(42) {
    assert(session != nullptr && "session must not be null");
  }

  ~ParityOS_QDMI_Device_Job_impl_d() {
    delete[] static_cast<char *>(program); // if allocated with new[]
  }
};

/// Trivial implementation because we provide a compilation service.
struct ParityOS_QDMI_Site_impl_d {};

/// Trivial implementation because we provide a compilation service.
struct ParityOS_QDMI_Operation_impl_d {};

//===----------------------------------------------------------------------===//
// QDMI device API implementation
//===----------------------------------------------------------------------===//



int ParityOS_QDMI_device_initialize(void) {
  CHECK_QDMI_ERROR(initialize_python());
  local_set_device_status(QDMI_DEVICE_STATUS_IDLE);
  return QDMI_SUCCESS;
}

int ParityOS_QDMI_device_finalize(void) {
  local_set_device_status(QDMI_DEVICE_STATUS_OFFLINE);

  if (*get_parityos_module() != nullptr) {
    Py_DECREF(*get_parityos_module());
    *get_parityos_module() = nullptr;
  }

  if (Py_IsInitialized() && !_Py_IsFinalizing() && !is_from_python()) {
    PyGILState_STATE gstate = PyGILState_Ensure();
    Py_Finalize();
  }

  return QDMI_SUCCESS;
}

int ParityOS_QDMI_device_session_alloc(ParityOS_QDMI_Device_Session *session) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  *session = new ParityOS_QDMI_Device_Session_impl_d();

  return QDMI_SUCCESS;
}

void ParityOS_QDMI_device_session_free(ParityOS_QDMI_Device_Session session) {
  if (session != nullptr) {
    delete session;
    session = nullptr;
  }
}

int ParityOS_QDMI_device_session_init(ParityOS_QDMI_Device_Session session) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  switch (local_read_device_status()) {
  case QDMI_DEVICE_STATUS_ERROR:
  case QDMI_DEVICE_STATUS_OFFLINE:
  case QDMI_DEVICE_STATUS_MAINTENANCE:
    return QDMI_ERROR_FATAL;
  default:
    break;
  }

  /// FIXME: implement authentication!!!!!!!
  if (session->username != "admin") {
    return QDMI_ERROR_PERMISSIONDENIED;
  }

  /// FIXME: Add meaningful implementation

  session->status = SESSION_STATUS::INITIALIZED;
  return QDMI_SUCCESS;
}

int ParityOS_QDMI_device_session_set_parameter(
    ParityOS_QDMI_Device_Session session,
    const QDMI_Device_Session_Parameter param, const size_t size,
    const void *value) {
  if (session == nullptr || (value != nullptr && size == 0) ||
      (param >= QDMI_DEVICE_SESSION_PARAMETER_MAX &&
       param != QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1 &&
       param != QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2 &&
       param != QDMI_DEVICE_SESSION_PARAMETER_CUSTOM3 &&
       param != QDMI_DEVICE_SESSION_PARAMETER_CUSTOM4 &&
       param != QDMI_DEVICE_SESSION_PARAMETER_CUSTOM5)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  if (session->status != SESSION_STATUS::ALLOCATED) {
    /// Ensures that this is called before ParityOS_QDMI_device_session_init.
    return QDMI_ERROR_BADSTATE;
  }

  if (param != QDMI_DEVICE_SESSION_PARAMETER_BASEURL &&
      param != QDMI_DEVICE_SESSION_PARAMETER_USERNAME) {
    return QDMI_ERROR_NOTSUPPORTED;
  }

  if (value != nullptr) {
    switch (param) {
    case QDMI_DEVICE_SESSION_PARAMETER_BASEURL:
      session->base_url = std::string(static_cast<const char *>(value), size);
      break;
    case QDMI_DEVICE_SESSION_PARAMETER_USERNAME:
      session->username = std::string(static_cast<const char *>(value), size);
      break;
    default:
      return QDMI_ERROR_NOTSUPPORTED;
    }
  }

  return QDMI_SUCCESS;
}

int ParityOS_QDMI_device_session_create_device_job(
    ParityOS_QDMI_Device_Session session, ParityOS_QDMI_Device_Job *job) {
  if (session == nullptr || job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  if (session->status != SESSION_STATUS::INITIALIZED) {
    return QDMI_ERROR_BADSTATE;
  }

  /// FIXME: probably want to create some meaningful job here (can be a dummy)

  *job = new ParityOS_QDMI_Device_Job_impl_d(session);

  return QDMI_SUCCESS;
}

void ParityOS_QDMI_device_job_free(ParityOS_QDMI_Device_Job job) { delete job; }

int ParityOS_QDMI_device_job_set_parameter(
    ParityOS_QDMI_Device_Job job, const QDMI_Device_Job_Parameter param,
    const size_t size, const void *value) {
  if (job == nullptr || (value != nullptr && size == 0) ||
      (param >= QDMI_DEVICE_JOB_PARAMETER_MAX &&
       param != QDMI_DEVICE_JOB_PARAMETER_CUSTOM1 &&
       param != QDMI_DEVICE_JOB_PARAMETER_CUSTOM2 &&
       param != QDMI_DEVICE_JOB_PARAMETER_CUSTOM3 &&
       param != QDMI_DEVICE_JOB_PARAMETER_CUSTOM4 &&
       param != QDMI_DEVICE_JOB_PARAMETER_CUSTOM5)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  if (job->status != QDMI_JOB_STATUS_CREATED) {
    return QDMI_ERROR_BADSTATE;
  }

  switch (param) {
  case QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT:
    if (value != nullptr) {
      const auto format = *static_cast<const QDMI_Program_Format *>(value);

      if (format >= QDMI_PROGRAM_FORMAT_MAX &&
          format != QDMI_PROGRAM_FORMAT_CUSTOM1 &&
          format != QDMI_PROGRAM_FORMAT_CUSTOM2 &&
          format != QDMI_PROGRAM_FORMAT_CUSTOM3 &&
          format != QDMI_PROGRAM_FORMAT_CUSTOM4 &&
          format != QDMI_PROGRAM_FORMAT_CUSTOM5) {
        return QDMI_ERROR_INVALIDARGUMENT;
      }

      /// TODO: our job format is the json form of the problem representation
      if (format != QDMI_PROGRAM_FORMAT_CUSTOM1) {
        return QDMI_ERROR_NOTSUPPORTED;
      }

      job->format = format;
    }
    return QDMI_SUCCESS;
  case QDMI_DEVICE_JOB_PARAMETER_PROGRAM:
    if (value != nullptr) {
      job->program = new char[size];
      memcpy(job->program, value, size);
    }
    return QDMI_SUCCESS;
  default:
    return QDMI_ERROR_NOTSUPPORTED;
  }
}

int ParityOS_QDMI_device_job_query_property(ParityOS_QDMI_Device_Job job,
                                            const QDMI_Device_Job_Property prop,
                                            const size_t size, void *value,
                                            size_t *size_ret) {
  if (job == nullptr || (value != nullptr && size == 0) ||
      (prop >= QDMI_DEVICE_JOB_PROPERTY_MAX &&
       prop != QDMI_DEVICE_JOB_PROPERTY_CUSTOM1 &&
       prop != QDMI_DEVICE_JOB_PROPERTY_CUSTOM2 &&
       prop != QDMI_DEVICE_JOB_PROPERTY_CUSTOM3 &&
       prop != QDMI_DEVICE_JOB_PROPERTY_CUSTOM4 &&
       prop != QDMI_DEVICE_JOB_PROPERTY_CUSTOM5)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  /// FIXME: should I already do something in the job query interface?

  const auto id_str = std::to_string(job->id);

  ADD_STRING_PROPERTY(QDMI_DEVICE_JOB_PROPERTY_ID, id_str.c_str(), prop, size,
                      value, size_ret)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_JOB_PROPERTY_PROGRAMFORMAT,
                            QDMI_Program_Format, job->format, prop, size, value,
                            size_ret)

  return QDMI_ERROR_NOTSUPPORTED;
}

int ParityOS_QDMI_device_job_submit(ParityOS_QDMI_Device_Job job) {
  // According to interface:
  if (job == nullptr || job->status != QDMI_JOB_STATUS_CREATED) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  // Not explicitly mentioned by interface:
  if (job->status != QDMI_JOB_STATUS_CREATED || job->program == nullptr)
    return QDMI_ERROR_INVALIDARGUMENT;

  job->status = QDMI_JOB_STATUS_SUBMITTED;

  /// FIXME: here we have to reach out to our python client. We probably want an
  /// async call here. The interface itself allows both: sync and async.
  int err = 1;

  if (err) {
    /// Here I assume that err means that submission itself failed.
    job->status = QDMI_JOB_STATUS_FAILED;
    return QDMI_ERROR_FATAL;
  }

  /// TODO: Not exactly sure what "busy" should mean and how to make sure it is
  /// reset reliably once the computation comes back.
  local_set_device_status(QDMI_DEVICE_STATUS_BUSY);

  return QDMI_SUCCESS;
}

int ParityOS_QDMI_device_job_cancel(ParityOS_QDMI_Device_Job job) {
  if (job == nullptr || job->status == QDMI_JOB_STATUS_DONE) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  /// FIXME: can we cancel our jobs?

  job->status = QDMI_JOB_STATUS_CANCELED;
  local_set_device_status(QDMI_DEVICE_STATUS_IDLE);
  return QDMI_SUCCESS;
}

int ParityOS_QDMI_device_job_check(ParityOS_QDMI_Device_Job job,
                                   QDMI_Job_Status *status) {
  if (job == nullptr || status == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  /// FIXME: implement this.
  local_set_device_status(QDMI_DEVICE_STATUS_IDLE);
  job->status = QDMI_JOB_STATUS_FAILED;
  *status = job->status;

  return QDMI_SUCCESS;
}

int ParityOS_QDMI_device_job_wait(ParityOS_QDMI_Device_Job job,
                                  [[maybe_unused]] const size_t timeout) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  /// FIXME: implement wait for job properly (maybe)
  return QDMI_ERROR_FATAL;
}

int ParityOS_QDMI_device_job_get_results(ParityOS_QDMI_Device_Job job,
                                         const QDMI_Job_Result result,
                                         const size_t size, void *data,
                                         size_t *size_ret) {
  if (job == nullptr || job->status != QDMI_JOB_STATUS_DONE ||
      (data != nullptr && size == 0) ||
      (result >= QDMI_JOB_RESULT_MAX && result != QDMI_JOB_RESULT_CUSTOM1 &&
       result != QDMI_JOB_RESULT_CUSTOM2 && result != QDMI_JOB_RESULT_CUSTOM3 &&
       result != QDMI_JOB_RESULT_CUSTOM4 &&
       result != QDMI_JOB_RESULT_CUSTOM5)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  /// FIXME: implement getting the job result

  switch (result) {
  case QDMI_JOB_RESULT_CUSTOM1:
    /// TODO: we need custom results right now as nothing else fits.
  default:
    return QDMI_ERROR_NOTSUPPORTED;
  }
}

int ParityOS_QDMI_device_session_query_device_property(
    ParityOS_QDMI_Device_Session session, const QDMI_Device_Property prop,
    const size_t size, void *value, size_t *size_ret) {
  if (prop >= QDMI_DEVICE_PROPERTY_MAX ||
      (value == nullptr && size_ret == nullptr)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  /// TODO: we do not have terribly important info to provide. But those entries
  /// are just adhoc value. Reconsider them at some point.

  // TODO: Bettern name?
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_NAME, "ParityOS", prop, size, value,
                      size_ret)
  // FIXME: Version of parityos?
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_VERSION, "3.0.0", prop, size, value,
                      size_ret)
  // TODO: Version of QDMI. Hardcode at implementation time? Retrieve from
  // somewhere?
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_LIBRARYVERSION, "1.2.0", prop, size,
                      value, size_ret)

  /// This is probably the only interesting info we can provide:
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_STATUS, QDMI_Device_Status,
                            local_read_device_status(), prop, size, value,
                            size_ret)

  // Anything else refers to a quantum computer.
  return QDMI_ERROR_NOTSUPPORTED;
}

int ParityOS_QDMI_device_session_query_site_property(
    ParityOS_QDMI_Device_Session session, ParityOS_QDMI_Site site,
    const QDMI_Site_Property prop, const size_t size, void *value,
    size_t *size_ret) {
  if (session == nullptr || site == nullptr ||
      (value != nullptr && size == 0) ||
      (prop >= QDMI_SITE_PROPERTY_MAX && prop != QDMI_SITE_PROPERTY_CUSTOM1 &&
       prop != QDMI_SITE_PROPERTY_CUSTOM2 &&
       prop != QDMI_SITE_PROPERTY_CUSTOM3 &&
       prop != QDMI_SITE_PROPERTY_CUSTOM4 &&
       prop != QDMI_SITE_PROPERTY_CUSTOM5)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  assert(false && "unreachable: user cannot create a site (in a normal way).");

  return QDMI_ERROR_NOTSUPPORTED;
}

int ParityOS_QDMI_device_session_query_operation_property(
    ParityOS_QDMI_Device_Session session, ParityOS_QDMI_Operation operation,
    const size_t num_sites, const ParityOS_QDMI_Site *sites,
    const size_t num_params, const double *params, QDMI_Operation_Property prop,
    const size_t size, void *value, size_t *size_ret) {
  if (session == nullptr || operation == nullptr ||
      (sites != nullptr && num_sites == 0) ||
      (params != nullptr && num_params == 0) ||
      (value != nullptr && size == 0) ||
      (prop >= QDMI_OPERATION_PROPERTY_MAX &&
       prop != QDMI_OPERATION_PROPERTY_CUSTOM1 &&
       prop != QDMI_OPERATION_PROPERTY_CUSTOM2 &&
       prop != QDMI_OPERATION_PROPERTY_CUSTOM3 &&
       prop != QDMI_OPERATION_PROPERTY_CUSTOM4 &&
       prop != QDMI_OPERATION_PROPERTY_CUSTOM5)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  assert(false &&
         "unreachable: user cannot create an operation (in a normal way).");

  return QDMI_ERROR_NOTSUPPORTED;
}
