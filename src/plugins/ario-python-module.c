/*
 * heavily based on code from Gedit
 *
 * Copyright (C) 2005 Raphael Slinckx
 * Copyright (C) 2008 Marc Pavot <marc.pavot@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301  USA.
 */

#include "ario-python-module.h"
#include <config.h>
#include <pygobject.h>
#include <pygtk/pygtk.h>

#include <signal.h>

#include <gmodule.h>

#include "ario-python-plugin.h"
#include "ario-debug.h"

#if PY_VERSION_HEX < 0x02050000
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif

#define ARIO_PYTHON_MODULE_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), \
                                                 ARIO_TYPE_PYTHON_MODULE, \
                                                 ArioPythonModulePrivate))

struct _ArioPythonModulePrivate
{
        gchar *module;
        gchar *path;
        GType type;
};

enum
{
        PROP_0,
        PROP_PATH,
        PROP_MODULE
};

/* Exported by pyario module */
void pyario_register_classes (PyObject *d);
void pyario_add_constants (PyObject *module, const gchar *strip_prefix);
extern PyMethodDef pyario_functions[];

/* We retreive this to check for correct class hierarchy */
static PyTypeObject *PyArioPlugin_Type;

G_DEFINE_TYPE (ArioPythonModule, ario_python_module, G_TYPE_TYPE_MODULE)

static gboolean
ario_python_module_load (GTypeModule *gmodule)
{
        ArioPythonModulePrivate *priv = ARIO_PYTHON_MODULE_GET_PRIVATE (gmodule);
        PyObject *main_module, *main_locals, *locals, *key, *value;
        PyObject *module, *fromlist;
        Py_ssize_t pos = 0;
        
        g_return_val_if_fail (Py_IsInitialized (), FALSE);

        main_module = PyImport_AddModule ("__main__");
        if (main_module == NULL)
        {
                g_warning ("Could not get __main__.");
                return FALSE;
        }

        /* If we have a special path, we register it */
        if (priv->path != NULL)
        {
                PyObject *sys_path = PySys_GetObject ("path");
                PyObject *path = PyString_FromString (priv->path);

                if (PySequence_Contains(sys_path, path) == 0)
                        PyList_Insert (sys_path, 0, path);

                Py_DECREF(path);
        }
        
        main_locals = PyModule_GetDict (main_module);

        /* we need a fromlist to be able to import modules with a '.' in the
           name. */
        fromlist = PyTuple_New(0);
        module = PyImport_ImportModuleEx (priv->module, main_locals, main_locals, fromlist); 
        Py_DECREF(fromlist);
        if (!module)
        {
                PyErr_Print ();
                return FALSE;
        }

        locals = PyModule_GetDict (module);
        while (PyDict_Next (locals, &pos, &key, &value))
        {
                if (!PyType_Check(value))
                        continue;

                if (PyObject_IsSubclass (value, (PyObject*) PyArioPlugin_Type))
                {
                        priv->type = ario_python_object_get_type (gmodule, value);
                        return TRUE;
                }
        }

        return FALSE;
}

static void
ario_python_module_unload (GTypeModule *module)
{
        ArioPythonModulePrivate *priv = ARIO_PYTHON_MODULE_GET_PRIVATE (module);
        ARIO_LOG_DBG ("Unloading Python module");
        
        priv->type = 0;
}

GObject *
ario_python_module_new_object (ArioPythonModule *module)
{
        ArioPythonModulePrivate *priv = ARIO_PYTHON_MODULE_GET_PRIVATE (module);
        ARIO_LOG_DBG ("Creating object of type %s", g_type_name (priv->type));

        if (priv->type == 0)
                return NULL;

        return g_object_new (priv->type, NULL);
}

static void
ario_python_module_init (ArioPythonModule *module)
{
        ARIO_LOG_DBG ("Init of Python module");
}

static void
ario_python_module_finalize (GObject *object)
{
        ArioPythonModulePrivate *priv = ARIO_PYTHON_MODULE_GET_PRIVATE (object);
        ARIO_LOG_DBG ("Finalizing Python module %s", g_type_name (priv->type));

        g_free (priv->module);
        g_free (priv->path);

        G_OBJECT_CLASS (ario_python_module_parent_class)->finalize (object);
}

static void
ario_python_module_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
        /* no readable properties */
        g_return_if_reached ();
}

static void
ario_python_module_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
        ArioPythonModule *mod = ARIO_PYTHON_MODULE (object);

        switch (prop_id)
        {
                case PROP_MODULE:
                        ARIO_PYTHON_MODULE_GET_PRIVATE (mod)->module = g_value_dup_string (value);
                        break;
                case PROP_PATH:
                        ARIO_PYTHON_MODULE_GET_PRIVATE (mod)->path = g_value_dup_string (value);
                        break;
                default:
                        g_return_if_reached ();
        }
}

static void
ario_python_module_class_init (ArioPythonModuleClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);
        GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (class);

        object_class->finalize = ario_python_module_finalize;
        object_class->get_property = ario_python_module_get_property;
        object_class->set_property = ario_python_module_set_property;

        g_object_class_install_property
                        (object_class,
                         PROP_MODULE,
                         g_param_spec_string ("module",
                                              "Module Name",
                                              "The Python module to load for this plugin",
                                              NULL,
                                              G_PARAM_READWRITE |
                                              G_PARAM_CONSTRUCT_ONLY |
                                              G_PARAM_STATIC_STRINGS));

        g_object_class_install_property
                        (object_class,
                         PROP_PATH,
                         g_param_spec_string ("path",
                                              "Path",
                                              "The Python path to use when loading this module",
                                              NULL,
                                              G_PARAM_READWRITE |
                                              G_PARAM_CONSTRUCT_ONLY |
                                              G_PARAM_STATIC_STRINGS));

        g_type_class_add_private (object_class, sizeof (ArioPythonModulePrivate));
        
        module_class->load = ario_python_module_load;
        module_class->unload = ario_python_module_unload;
}

ArioPythonModule *
ario_python_module_new (const gchar *path,
                         const gchar *module)
{
        ArioPythonModule *result;

        if (module == NULL || module[0] == '\0')
                return NULL;

        result = g_object_new (ARIO_TYPE_PYTHON_MODULE,
                               "module", module,
                               "path", path,
                               NULL);

        g_type_module_set_name (G_TYPE_MODULE (result), module);

        return result;
}


/* --- these are not module methods, they are here out of convenience --- */

static gint idle_garbage_collect_id = 0;

/* C equivalent of
 *    import pygtk
 *    pygtk.require ("2.0")
 */
static gboolean
check_pygtk2 (void)
{
        PyObject *pygtk, *mdict, *require;

        /* pygtk.require("2.0") */
        pygtk = PyImport_ImportModule ("pygtk");
        if (pygtk == NULL)
        {
                g_warning ("Error initializing Python interpreter: could not import pygtk.");
                return FALSE;
        }

        mdict = PyModule_GetDict (pygtk);
        require = PyDict_GetItemString (mdict, "require");
        PyObject_CallObject (require,
                             Py_BuildValue ("(S)", PyString_FromString ("2.0")));
        if (PyErr_Occurred())
        {
                g_warning ("Error initializing Python interpreter: pygtk 2 is required.");
                return FALSE;
        }

        return TRUE;
}

/* Note: the following two functions are needed because
 * init_pyobject and init_pygtk which are *macros* which in case
 * case of error set the PyErr and then make the calling
 * function return behind our back.
 * It's up to the caller to check the result with PyErr_Occurred()
 */
static void
ario_init_pygobject (void)
{
        init_pygobject_check (2, 11, 5); /* FIXME: get from config */
}

static void
ario_init_pygtk (void)
{
        PyObject *gtk, *mdict, *version, *required_version;

        init_pygtk ();

        /* there isn't init_pygtk_check(), do the version
         * check ourselves */
        gtk = PyImport_ImportModule("gtk");
        mdict = PyModule_GetDict(gtk);
        version = PyDict_GetItemString (mdict, "pygtk_version");
        if (!version)
        {
                PyErr_SetString (PyExc_ImportError,
                                 "PyGObject version too old");
                return;
        }

        required_version = Py_BuildValue ("(iii)", 2, 4, 0); /* FIXME */

        if (PyObject_Compare (version, required_version) == -1)
        {
                PyErr_SetString (PyExc_ImportError,
                                 "PyGObject version too old");
                Py_DECREF (required_version);
                return;
        }

        Py_DECREF (required_version);
}

gboolean
ario_python_init (void)
{
        PyObject *mdict, *tuple;
        PyObject *ario;
        PyObject *gettext, *install, *gettext_args;
        struct sigaction old_sigint;
        gint res;
        char *argv[] = { "ario", NULL };
        
        static gboolean init_failed = FALSE;

        if (init_failed)
        {
                /* We already failed to initialized Python, don't need to
                 * retry again */
                return FALSE;
        }
        
        if (Py_IsInitialized ())
        {
                /* Python has already been successfully initialized */
                return TRUE;
        }

        /* We are trying to initialize Python for the first time,
           set init_failed to FALSE only if the entire initialization process
           ends with success */
        init_failed = TRUE;

        /* Hack to make python not overwrite SIGINT: this is needed to avoid
         * the crash reported on bug #326191 */

        /* CHECK: can't we use Py_InitializeEx instead of Py_Initialize in order
          to avoid to manage signal handlers ? - Paolo (Dec. 31, 2006) */

        /* Save old handler */
        res = sigaction (SIGINT, NULL, &old_sigint);  
        if (res != 0)
        {
                g_warning ("Error initializing Python interpreter: cannot get "
                           "handler to SIGINT signal (%s)",
                           strerror (errno));

                return FALSE;
        }

        /* Python initialization */
        Py_Initialize ();

        /* Restore old handler */
        res = sigaction (SIGINT, &old_sigint, NULL);
        if (res != 0)
        {
                g_warning ("Error initializing Python interpreter: cannot restore "
                           "handler to SIGINT signal (%s).",
                           strerror (errno));
                goto python_init_error;
        }

        PySys_SetArgv (1, argv);

        if (!check_pygtk2 ())
        {
                /* Warning message already printed in check_pygtk2 */
                goto python_init_error;
        }

        /* import gobject */        
        ario_init_pygobject ();
        if (PyErr_Occurred ())
        {
                g_warning ("Error initializing Python interpreter: could not import pygobject.");
                goto python_init_error;                
        }

        /* import gtk */
        ario_init_pygtk ();
        if (PyErr_Occurred ())
        {
                g_warning ("Error initializing Python interpreter: could not import pygtk.");
                goto python_init_error;
        }
        
        /* import ario */
        ario = Py_InitModule ("ario", pyario_functions);
        mdict = PyModule_GetDict (ario);

        pyario_register_classes (mdict);
        pyario_add_constants (ario, "ARIO_");

        /* ario version */
        tuple = Py_BuildValue("(i)", 
                              PACKAGE_VERSION);
        PyDict_SetItemString(mdict, "version", tuple);
        Py_DECREF(tuple);
        
        /* Retrieve the Python type for ario.Plugin */
        PyArioPlugin_Type = (PyTypeObject *) PyDict_GetItemString (mdict, "Plugin"); 
        if (PyArioPlugin_Type == NULL)
        {
                g_warning ("Error initializing Python interpreter: could not get Plugin item.");
                PyErr_Print ();
                goto python_init_error;
        }

        /* i18n support */
        gettext = PyImport_ImportModule ("gettext");
        if (gettext == NULL)
        {
                g_warning ("Error initializing Python interpreter: could not import gettext.");
                goto python_init_error;
        }

        mdict = PyModule_GetDict (gettext);
        install = PyDict_GetItemString (mdict, "install");
        gettext_args = Py_BuildValue ("ss", GETTEXT_PACKAGE, LOCALE_DIR);
        PyObject_CallObject (install, gettext_args);
        Py_DECREF (gettext_args);
        
        /* Python has been successfully initialized */
        init_failed = FALSE;
        
        return TRUE;
        
python_init_error:

        g_warning ("Please check the installation of all the Python related packages required "
                   "by ario and try again.");

        PyErr_Clear ();

        ario_python_shutdown ();

        return FALSE;
}

void
ario_python_shutdown (void)
{
        if (Py_IsInitialized ())
        {
                if (idle_garbage_collect_id != 0)
                {
                        g_source_remove (idle_garbage_collect_id);
                        idle_garbage_collect_id = 0;
                }

                while (PyGC_Collect ())
                        ;        

                Py_Finalize ();
        }
}

static gboolean
run_gc (gpointer data)
{
        while (PyGC_Collect ())
                ;

        idle_garbage_collect_id = 0;
        return FALSE;
}

void
ario_python_garbage_collect (void)
{
        if (Py_IsInitialized())
        {
                /*
                 * We both run the GC right now and we schedule
                 * a further collection in the main loop.
                 */

                while (PyGC_Collect ())
                        ;

                if (idle_garbage_collect_id == 0)
                        idle_garbage_collect_id = g_idle_add (run_gc, NULL);
        }
}

