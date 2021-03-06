////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Arc Welder: Anti-Stutter Python Extension for the OctoPrint Arc Welder plugin.
//
// Compresses many G0/G1 commands into G2/G3(arc) commands where possible, ensuring the tool paths stay within the specified resolution.
// This reduces file size and the number of gcodes per second.
//
// Copyright(C) 2020 - Brad Hochgesang
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This program is free software : you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU Affero General Public License for more details.
//
//
// You can contact the author at the following email address: 
// FormerLurker@pm.me
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "py_arc_welder_extension.h"
#include "py_arc_welder.h"
#include <iomanip>
#include <sstream>
#include <iostream>
#include "arc_welder.h"
#include "py_logger.h"
#include "python_helpers.h"
#include "version.h"

#if PY_MAJOR_VERSION >= 3
int main(int argc, char* argv[])
{
	wchar_t* program = Py_DecodeLocale(argv[0], NULL);
	if (program == NULL) {
		fprintf(stderr, "Fatal error: cannot decode argv[0]\n");
		exit(1);
	}

	// Add a built-in module, before Py_Initialize
	PyImport_AppendInittab("PyArcWelder", PyInit_PyArcWelder);

	// Pass argv[0] to the Python interpreter
	Py_SetProgramName(program);

	// Initialize the Python interpreter.  Required.
	Py_Initialize();
	// We are not using threads, do not enable.
	//std::cout << "Initializing threads...";
	//if (!PyEval_ThreadsInitialized()) {
	//	PyEval_InitThreads();
	//}
	// Optionally import the module; alternatively, import can be deferred until the embedded script imports it.
	PyImport_ImportModule("PyArcWelder");
	PyMem_RawFree(program);
	return 0;
}

#else

int main(int argc, char* argv[])
{
	Py_SetProgramName(argv[0]);
	Py_Initialize();
	// We are not using threads, do not enable.
	/* std::cout << "Initializing threads...";
	if (!PyEval_ThreadsInitialized()) {
		PyEval_InitThreads();
	}
	*/
	initPyArcWelder();
	return 0;

}
#endif

struct module_state {
	PyObject* error;
};
#if PY_MAJOR_VERSION >= 3
#define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))
#else
#define GETSTATE(m) (&_state)
static struct module_state _state;
#endif

// Python 2 module method definition
static PyMethodDef PyArcWelderMethods[] = {
	{ "ConvertFile", (PyCFunction)ConvertFile,  METH_VARARGS  ,"Converts segmented curve approximations to actual G2/G3 arcs within the supplied resolution." },
	{ NULL, NULL, 0, NULL }
};

// Python 3 module method definition
#if PY_MAJOR_VERSION >= 3
static int PyArcWelder_traverse(PyObject* m, visitproc visit, void* arg) {
	Py_VISIT(GETSTATE(m)->error);
	return 0;
}

static int PyArcWelder_clear(PyObject* m) {
	Py_CLEAR(GETSTATE(m)->error);
	return 0;
}

static struct PyModuleDef moduledef = {
		PyModuleDef_HEAD_INIT,
		"PyArcWelder",
		NULL,
		sizeof(struct module_state),
		PyArcWelderMethods,
		NULL,
		PyArcWelder_traverse,
		PyArcWelder_clear,
		NULL
};

#define INITERROR return NULL

PyMODINIT_FUNC
PyInit_PyArcWelder(void)

#else
#define INITERROR return

extern "C" void initPyArcWelder(void)
#endif
{
	std::cout << "Initializing PyArcWelder";
	std::cout << "\nVersion: " << GIT_TAGGED_VERSION << ", Branch: " << GIT_BRANCH << ", BuildDate: " << BUILD_DATE;
	std::cout << "\nCopyright(C) " << COPYRIGHT_DATE << " - " << AUTHOR;
#if PY_MAJOR_VERSION >= 3
	std::cout << " Python 3+ Detected...";
	PyObject* module = PyModule_Create(&moduledef);
#else
	std::cout << " Python 2 Detected...";
	PyObject* module = Py_InitModule("PyArcWelder", PyArcWelderMethods);
#endif

	if (module == NULL)
		INITERROR;
	struct module_state* st = GETSTATE(module);

	st->error = PyErr_NewException((char*)"PyArcWelder.Error", NULL, NULL);
	if (st->error == NULL) {
		Py_DECREF(module);
		INITERROR;
	}
	std::vector<std::string> logger_names;
	logger_names.push_back("arc_welder.gcode_conversion");
	std::vector<int> logger_levels;
	logger_levels.push_back(log_levels::NOSET);
	logger_levels.push_back(log_levels::VERBOSE);
	logger_levels.push_back(log_levels::DEBUG);
	logger_levels.push_back(log_levels::INFO);
	logger_levels.push_back(log_levels::WARNING);
	logger_levels.push_back(log_levels::ERROR);
	logger_levels.push_back(log_levels::CRITICAL);
	p_py_logger = new py_logger(logger_names, logger_levels);
	p_py_logger->initialize_loggers();
	p_py_logger->set_log_level(INFO);
	std::cout << " Initialization Complete\r\n";

#if PY_MAJOR_VERSION >= 3
	return module;
#endif
}

extern "C"
{
	static PyObject* ConvertFile(PyObject* self, PyObject* py_args)
	{
		PyObject* py_convert_file_args;
		if (!PyArg_ParseTuple(
			py_args,
			"O",
			&py_convert_file_args
			))
		{
			std::string message = "py_gcode_arc_converter.ConvertFile - Cound not extract the parameters dictionary.";
			p_py_logger->log_exception(GCODE_CONVERSION, message);
			return NULL;
		}

		py_gcode_arc_args args;
		PyObject* py_progress_callback = NULL;
		
		if (!ParseArgs(py_convert_file_args, args, &py_progress_callback))
		{
			return NULL;
		}
		p_py_logger->set_log_level(args.log_level);

		std::string message = "py_gcode_arc_converter.ConvertFile - Beginning Arc Conversion.";
		p_py_logger->log(GCODE_CONVERSION, INFO, message);

		py_arc_welder arc_welder_obj(
			args.guid, 
			args.source_path, 
			args.target_path, 
			p_py_logger, 
			args.resolution_mm, 
			args.path_tolerance_percent, 
			args.max_radius_mm, 
			args.min_arc_segments, 
			args.mm_per_arc_segment, 
			args.g90_g91_influences_extruder, 
			args.allow_3d_arcs, 
			args.allow_dynamic_precision, 
			args.default_xyz_precision, 
			args.default_e_precision, 
			DEFAULT_GCODE_BUFFER_SIZE, 
			py_progress_callback
		);
		arc_welder_results results = arc_welder_obj.process();
		message = "py_gcode_arc_converter.ConvertFile - Arc Conversion Complete.";
		p_py_logger->log(GCODE_CONVERSION, INFO, message);
		Py_XDECREF(py_progress_callback);
		// return the arguments
		PyObject* p_progress = py_arc_welder::build_py_progress(results.progress, args.guid);
		if (p_progress == NULL)
			p_progress = Py_None;

		PyObject* p_results = Py_BuildValue(
			"{s:i,s:i,s:s,s:O}",
			"success",
			(long int)(results.success ? 1 : 0),
			"is_cancelled",
			(long int)(results.cancelled ? 1 : 0),
			"message",
			results.message.c_str(),
			"progress",
			p_progress
		);
		return p_results;
	}
}

static bool ParseArgs(PyObject* py_args, py_gcode_arc_args& args, PyObject** py_progress_callback)
{
	p_py_logger->log(
		GCODE_CONVERSION, INFO,
		"Parsing GCode Conversion Args."
		);

	// Extract the job guid
	PyObject* py_guid = PyDict_GetItemString(py_args, "guid");
	if (py_guid == NULL)
	{
		std::string message = "ParseArgs - Unable to retrieve the guid parameter from the args.";
		p_py_logger->log_exception(GCODE_CONVERSION, message);
		return false;
	}
	args.guid = gcode_arc_converter::PyUnicode_SafeAsString(py_guid);

	// Extract the source file path
	PyObject* py_source_path =  PyDict_GetItemString(py_args, "source_path");
	if (py_source_path == NULL)
	{
		std::string message = "ParseArgs - Unable to retrieve the source_path parameter from the args.";
		p_py_logger->log_exception(GCODE_CONVERSION, message);
		return false;
	}
	args.source_path = gcode_arc_converter::PyUnicode_SafeAsString(py_source_path);

	// Extract the target file path
	PyObject* py_target_path = PyDict_GetItemString(py_args, "target_path");
	if (py_target_path == NULL)
	{
		std::string message = "ParseArgs - Unable to retrieve the target_path parameter from the args.";
		p_py_logger->log_exception(GCODE_CONVERSION, message);
		return false;
	}
	args.target_path = gcode_arc_converter::PyUnicode_SafeAsString(py_target_path);

	// Extract the resolution in millimeters
	PyObject* py_resolution_mm = PyDict_GetItemString(py_args, "resolution_mm");
	if (py_resolution_mm == NULL)
	{
		std::string message = "ParseArgs - Unable to retrieve the resolution_mm parameter from the args.";
		p_py_logger->log_exception(GCODE_CONVERSION, message);
		return false;
	}
	args.resolution_mm = gcode_arc_converter::PyFloatOrInt_AsDouble(py_resolution_mm);
	if (args.resolution_mm <= 0)
	{
		args.resolution_mm = 0.05; // Set to the default if no resolution is provided, or if it is less than 0.
	}

	// extract allow_dynamic_precision
	PyObject* py_allow_dynamic_precision = PyDict_GetItemString(py_args, "allow_dynamic_precision");
	if (py_allow_dynamic_precision == NULL)
	{
		std::string message = "ParseArgs - Unable to retrieve allow_dynamic_precision from the args.";
		p_py_logger->log_exception(GCODE_CONVERSION, message);
		return false;
	}
	args.allow_dynamic_precision = PyLong_AsLong(py_allow_dynamic_precision) > 0;

	// extract default_xyz_precision
	PyObject* py_default_xyz_precision = PyDict_GetItemString(py_args, "default_xyz_precision");
	if (py_default_xyz_precision == NULL)
	{
		std::string message = "ParseArgs - Unable to retrieve the default_xyz_precision parameter from the args.";
		p_py_logger->log_exception(GCODE_CONVERSION, message);
		return false;
	}
	args.default_xyz_precision = gcode_arc_converter::PyFloatOrInt_AsDouble(py_default_xyz_precision);
	if (args.default_xyz_precision < 3)
	{
		std::string message = "ParseArgs - The default XYZ precision received was less than 3, which could cause problems printing arcs.  Setting to 3.";
		p_py_logger->log(WARNING, GCODE_CONVERSION, message);
		args.default_xyz_precision = 3;
	}
	else if (args.default_xyz_precision > 6)
	{
		std::string message = "ParseArgs - The default XYZ precision received was greater than 6, which could can cause checksum errors depending on your firmware.  Setting to 6.";
		p_py_logger->log(WARNING, GCODE_CONVERSION, message);
		args.default_xyz_precision = 6;
	}

	// extract default_e_precision
	PyObject* py_default_e_precision = PyDict_GetItemString(py_args, "default_e_precision");
	if (py_default_e_precision == NULL)
	{
		std::string message = "ParseArgs - Unable to retrieve the default_e_precision parameter from the args.";
		p_py_logger->log_exception(GCODE_CONVERSION, message);
		return false;
	}
	args.default_e_precision = gcode_arc_converter::PyFloatOrInt_AsDouble(py_default_e_precision);
	if (args.default_e_precision < 3)
	{
		std::string message = "ParseArgs - The default E precision received was less than 3, which could cause extrusion problems.  Setting to 3.";
		p_py_logger->log(WARNING, GCODE_CONVERSION, message);
		args.default_e_precision = 3;
	}
	else if (args.default_e_precision > 6)
	{
		std::string message = "ParseArgs - The default E precision received was greater than 6, which could can cause checksum errors depending on your firmware.  Setting to 6.";
		p_py_logger->log(WARNING, GCODE_CONVERSION, message);
		args.default_e_precision = 6;
	}

	// Extract the path tolerance_percent
	PyObject* py_path_tolerance_percent = PyDict_GetItemString(py_args, "path_tolerance_percent");
	if (py_path_tolerance_percent == NULL)
	{
		std::string message = "ParseArgs - Unable to retrieve the py_path_tolerance_percent parameter from the args.";
		p_py_logger->log_exception(GCODE_CONVERSION, message);
		return false;
	}
	args.path_tolerance_percent = gcode_arc_converter::PyFloatOrInt_AsDouble(py_path_tolerance_percent);
	if (args.path_tolerance_percent < 0)
	{
		args.path_tolerance_percent = ARC_LENGTH_PERCENT_TOLERANCE_DEFAULT; // Set to the default if no resolution is provided, or if it is less than 0.
	}

	// Extract the max_radius in mm
	PyObject* py_max_radius_mm = PyDict_GetItemString(py_args, "max_radius_mm");
	if (py_max_radius_mm == NULL)
	{
		std::string message = "ParseArgs - Unable to retrieve the max_radius_mm parameter from the args.";
		p_py_logger->log_exception(GCODE_CONVERSION, message);
		return false;
	}
	args.max_radius_mm = gcode_arc_converter::PyFloatOrInt_AsDouble(py_max_radius_mm);
	if (args.max_radius_mm > DEFAULT_MAX_RADIUS_MM)
	{
		args.max_radius_mm = DEFAULT_MAX_RADIUS_MM; // Set to the default if no resolution is provided, or if it is less than 0.
	}

	// Extract the mm_per_arc_segment
	PyObject* py_mm_per_arc_segment = PyDict_GetItemString(py_args, "mm_per_arc_segment");
	if (py_mm_per_arc_segment == NULL)
	{
		std::string message = "ParseArgs - Unable to retrieve the mm_per_arc_segment parameter from the args.";
		p_py_logger->log_exception(GCODE_CONVERSION, message);
		return false;
	}
	args.mm_per_arc_segment = gcode_arc_converter::PyFloatOrInt_AsDouble(py_mm_per_arc_segment);
	if (args.mm_per_arc_segment < 0)
	{
		args.mm_per_arc_segment = 0;
	}

	// Extract min_arc_segments
	PyObject* py_min_arc_segments = PyDict_GetItemString(py_args, "min_arc_segments");
	if (py_min_arc_segments == NULL)
	{
		std::string message = "ParseArgs - Unable to retrieve the min_arc_segments parameter from the args.";
		p_py_logger->log_exception(GCODE_CONVERSION, message);
		return false;
	}
	args.min_arc_segments = (int) gcode_arc_converter::PyIntOrLong_AsLong(py_min_arc_segments);
	if (args.min_arc_segments < 0)
	{
		args.min_arc_segments = 0; // Set to the default if no resolution is provided, or if it is less than 0.
	}

	// extract allow_3d_arcs
	PyObject* py_allow_3d_arcs = PyDict_GetItemString(py_args, "allow_3d_arcs");
	if (py_allow_3d_arcs == NULL)
	{
		std::string message = "ParseArgs - Unable to retrieve allow_3d_arcs from the args.";
		p_py_logger->log_exception(GCODE_CONVERSION, message);
		return false;
	}
	args.allow_3d_arcs = PyLong_AsLong(py_allow_3d_arcs) > 0;

	// Extract G90/G91 influences extruder
	// g90_influences_extruder
	PyObject* py_g90_g91_influences_extruder = PyDict_GetItemString(py_args, "g90_g91_influences_extruder");
	if (py_g90_g91_influences_extruder == NULL)
	{
		std::string message = "ParseArgs - Unable to retrieve g90_g91_influences_extruder from the args.";
		p_py_logger->log_exception(GCODE_CONVERSION, message);
		return false;
	}
	args.g90_g91_influences_extruder = PyLong_AsLong(py_g90_g91_influences_extruder) > 0;

	// on_progress_received
	PyObject* py_on_progress_received = PyDict_GetItemString(py_args, "on_progress_received");
	if (py_on_progress_received == NULL)
	{
		std::string message = "ParseArgs - Unable to retrieve on_progress_received from the stabilization args.";
		p_py_logger->log_exception(GCODE_CONVERSION, message);
		return false;
	}
	// need to incref this so it doesn't vanish later (borrowed reference we are saving)
	Py_XINCREF(py_on_progress_received);
	*py_progress_callback = py_on_progress_received;

	// Extract log_level
	PyObject* py_log_level = PyDict_GetItemString(py_args, "log_level");
	if (py_log_level == NULL)
	{
		std::string message = "ParseArgs - Unable to retrieve log_level from the args.";
		p_py_logger->log_exception(GCODE_CONVERSION, message);
		return false;
	}
	
	int log_level_value = static_cast<int>(PyLong_AsLong(py_log_level));
	// determine the log level as an index rather than as a value
	args.log_level = p_py_logger->get_log_level_for_value(log_level_value);
	
	return true;
}

