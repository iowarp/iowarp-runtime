"""
Generate code for chimaera
"""

import os
import sys
from chimaera_codegen.util.paths import CHIMEARA_ROOT

class ChimaeraCodegen:
    def make_macro(self, PATH):
        """
        Converts the file at PATH into a C macro. The name of the
        file is used as the name of the macro. The macro name will
        be made all caps. You can use any extension on the file.
        """
        MACRO_NAME = os.path.basename(PATH).upper().split('.')[0]
        self.print_macro(PATH, MACRO_NAME)

    def print_macro(self, path, macro_name):
        """
        Prints the C macro conversion
        """
        with open(path) as fp:
            lines = fp.read().splitlines()
        macro_def = f'#define {macro_name}\\\n'
        macro_body = '\\\n'.join(lines)
        print(f'{macro_def}{macro_body}')

    def make_configs(self):
        """
        Creates the default chimaera client and server configurations
        """
        self._create_config(
            path=f"{CHIMEARA_ROOT}/config/chimaera_client_default.yaml",
            var_name="kChiDefaultClientConfigStr",
            config_path=f"{CHIMEARA_ROOT}/include/chimaera_codegen/config/config_client_default.h",
            macro_name="CHI_CLIENT"
        )
        self._create_config(
            path=f"{CHIMEARA_ROOT}/config/chimaera_server_default.yaml",
            var_name="kChiServerDefaultConfigStr",
            config_path=f"{CHIMEARA_ROOT}/include/chimaera_codegen/config/config_server_default.h",
            macro_name="CHI_SERVER"
        )

    def _create_config(self, path, var_name, config_path, macro_name):
        """
        Creates a chimaera configuration file. Either the server or the client.
        """
        with open(path) as fp:
            yaml_config_lines = fp.read().splitlines()

        # Create the hermes config string
        string_lines = []
        string_lines.append(f"const inline char* {var_name} = ")
        for line in yaml_config_lines:
            line = line.replace('\"', '\\\"')
            line = line.replace('\'', '\\\'')
            string_lines.append(f"\"{line}\\n\"")
        string_lines[-1] = string_lines[-1] + ';'

        # Create the configuration
        config_lines = []
        config_lines.append(f"#ifndef CHI_SRC_CONFIG_{macro_name}_DEFAULT_H_")
        config_lines.append(f"#define CHI_SRC_CONFIG_{macro_name}_DEFAULT_H_")
        config_lines += string_lines
        config_lines.append(f"#endif  // CHI_SRC_CONFIG_{macro_name}_DEFAULT_H_")

        # Persist
        config = "\n".join(config_lines)
        with open(config_path, 'w') as fp:
            fp.write(config)

    def make_task(self, TASK_ROOT):
        """
        Bootstraps a task. Copies all the necessary files and replaces. This
        is an aggressive operation.
        """
        TASK_TEMPL_ROOT = f'{CHIMEARA_ROOT}/tasks/TASK_NAME'
        TASK_NAME = os.path.basename(TASK_ROOT)
        if os.path.exists(f'{TASK_ROOT}/src'):
            ret = input('This task seems bootstrapped, do you really want to continue? (yes/no): ')
            if ret != 'yes':
                print('Skipping...')
                sys.exit(0)
        os.makedirs(f'{TASK_ROOT}/src', exist_ok=True)
        os.makedirs(f'{TASK_ROOT}/include/{TASK_NAME}', exist_ok=True)
        self._copy_replace(TASK_ROOT, TASK_TEMPL_ROOT, 'CMakeLists.txt', TASK_NAME)
        self._copy_replace(TASK_ROOT, TASK_TEMPL_ROOT, 'src/CMakeLists.txt', TASK_NAME)
        self._copy_replace(TASK_ROOT, TASK_TEMPL_ROOT, 'src/TASK_NAME.cc', TASK_NAME)
        self._copy_replace(TASK_ROOT, TASK_TEMPL_ROOT, 'src/TASK_NAME_monitor.py', TASK_NAME)
        self._copy_replace(TASK_ROOT, TASK_TEMPL_ROOT, 'include/TASK_NAME/TASK_NAME.h', TASK_NAME)
        self._copy_replace(TASK_ROOT, TASK_TEMPL_ROOT, 'include/TASK_NAME/TASK_NAME_lib_exec.h', TASK_NAME)
        self._copy_replace(TASK_ROOT, TASK_TEMPL_ROOT, 'include/TASK_NAME/TASK_NAME_tasks.h', TASK_NAME)
        self._copy_replace(TASK_ROOT, TASK_TEMPL_ROOT, 'include/TASK_NAME/TASK_NAME_methods.h', TASK_NAME)
        self._copy_replace(TASK_ROOT, TASK_TEMPL_ROOT, 'include/TASK_NAME/TASK_NAME_methods.yaml', TASK_NAME)

    def _copy_replace(self, TASK_ROOT, TASK_TEMPL_ROOT, rel_path, TASK_NAME):
        """
        Copies a file from TASK_TEMPL_ROOT to TASK_ROOT and renames
        TASK_TEMPL to the value of TASK_NAME
        """
        with open(f'{TASK_TEMPL_ROOT}/{rel_path}') as fp:
            text = fp.read()
        text = text.replace('TASK_NAME', TASK_NAME)
        rel_path = rel_path.replace('TASK_NAME', TASK_NAME)
        with open(f'{TASK_ROOT}/{rel_path}', 'w') as fp:
            fp.write(text)

    def refresh_repo_methods(self, TASK_REPO_DIR):
        TASK_ROOTS = [os.path.join(TASK_REPO_DIR, item)
                      for item in os.listdir(TASK_REPO_DIR)]
        for TASK_ROOT in TASK_ROOTS:
            try:
                self.refresh_methods(TASK_ROOT)
            except:
                pass

    def refresh_methods(self, TASK_ROOT):
        """
        Refreshes autogenerated code in the task.
        """
        if not os.path.exists(f'{TASK_ROOT}/include'):
            return
        MOD_NAME = os.path.basename(TASK_ROOT)
        METHODS_H = f'{TASK_ROOT}/include/{MOD_NAME}/{MOD_NAME}_methods.h'
        METHODS_YAML = f'{TASK_ROOT}/include/{MOD_NAME}/{MOD_NAME}_methods.yaml'
        LIB_EXEC_H = f'{TASK_ROOT}/include/{MOD_NAME}/{MOD_NAME}_lib_exec.h'
        NEW_TASKS_H = f'{TASK_ROOT}/include/{MOD_NAME}/_{MOD_NAME}_new_tasks.h'
        NEW_CLIENT_H = f'{TASK_ROOT}/include/{MOD_NAME}/_{MOD_NAME}_new_client.h'
        NEW_RUNTIME_H = f'{TASK_ROOT}/src/{MOD_NAME}/_{MOD_NAME}.cc'
        METHOD_MACRO = f'CHI_{MOD_NAME.upper()}_METHODS_H_'
        LIB_EXEC_MACRO = f'CHI_{MOD_NAME.upper()}_LIB_EXEC_H_'

        with open(METHODS_YAML) as fp:
            methods = yaml.load(fp, Loader=yaml.FullLoader)
        if methods is None:
            methods = {}
        methods = sorted(methods.items(), key=lambda x: x[1])

        # Produce the MOD_NAME_methods.h file
        lines = []
        lines += [f'#ifndef {METHOD_MACRO}',
                  f'#define {METHOD_MACRO}',
                  '',
                  '/** The set of methods in the admin task */',
                  'struct Method : public TaskMethod {']
        for method_enum_name, method_off in methods:
            if method_off < 10:
                continue
            lines += [f'  TASK_METHOD_T {method_enum_name} = {method_off};']
        lines += [f'  TASK_METHOD_T kCount = {methods[-1][1] + 1};']
        lines += ['};', '', f'#endif  // {METHOD_MACRO}']
        with open(METHODS_H, 'w') as fp:
            fp.write('\n'.join(lines))

        # Produce the MOD_NAME_lib_exec.h file
        lines = []
        lines += [f'#ifndef {LIB_EXEC_MACRO}',
                  f'#define {LIB_EXEC_MACRO}',
                  '']
        ## Create the Run method
        lines += ['/** Execute a task */',
                  'void Run(u32 method, Task *task, RunContext &rctx) override {',
                  '  switch (method) {']
        for method_enum_name, method_off in methods:
            if method_off < 0:
                continue
            method_name = method_enum_name.replace('k', '', 1)
            task_name = method_name + "Task"
            lines += [f'    case Method::{method_enum_name}: {{',
                      f'      {method_name}(reinterpret_cast<{task_name} *>(task), rctx);',
                      f'      break;',
                      f'    }}']
        lines += ['  }']
        lines += ['}']

        ## Create the Monitor method
        lines += ['/** Execute a task */',
                  'void Monitor(MonitorModeId mode, MethodId method, Task *task, RunContext &rctx) override {',
                  '  switch (method) {']
        for method_enum_name, method_off in methods:
            if method_off < 0:
                continue
            method_name = method_enum_name.replace('k', '', 1)
            task_name = method_name + "Task"
            lines += [f'    case Method::{method_enum_name}: {{',
                      f'      Monitor{method_name}(mode, reinterpret_cast<{task_name} *>(task), rctx);',
                      f'      break;',
                      f'    }}']
        lines += ['  }']
        lines += ['}']

        ## Create the Del method
        lines += ['/** Delete a task */',
                  'void Del(u32 method, Task *task) override {',
                  '  switch (method) {']
        for method_enum_name, method_off in methods:
            if method_off < 0:
                continue
            method_name = method_enum_name.replace('k', '', 1)
            task_name = method_name + "Task"
            lines += [f'    case Method::{method_enum_name}: {{',
                      f'      CHI_CLIENT->DelTask<{task_name}>(reinterpret_cast<{task_name} *>(task));',
                      f'      break;',
                      f'    }}']
        lines += ['  }']
        lines += ['}']

        ## Create the CopyStart method
        lines += ['/** Duplicate a task */',
                  'void CopyStart(u32 method, const Task *orig_task, Task *dup_task, bool deep) override {',
                  '  switch (method) {']
        for method_enum_name, method_off in methods:
            if method_off < 0:
                continue
            method_name = method_enum_name.replace('k', '', 1)
            task_name = method_name + "Task"
            lines += [f'    case Method::{method_enum_name}: {{',
                      f'      chi::CALL_COPY_START(',
                      f'        reinterpret_cast<const {task_name}*>(orig_task), ',
                      f'        reinterpret_cast<{task_name}*>(dup_task), deep);',
                      f'      break;',
                      f'    }}']
        lines += ['  }']
        lines += ['}']

        ## Create the CopyStart method
        lines += ['/** Duplicate a task */',
                  'void NewCopyStart(u32 method, const Task *orig_task, LPointer<Task> &dup_task, bool deep) override {',
                  '  switch (method) {']
        for method_enum_name, method_off in methods:
            if method_off < 0:
                continue
            method_name = method_enum_name.replace('k', '', 1)
            task_name = method_name + "Task"
            lines += [f'    case Method::{method_enum_name}: {{',
                      f'      chi::CALL_NEW_COPY_START(reinterpret_cast<const {task_name}*>(orig_task), dup_task, deep);',
                      f'      break;',
                      f'    }}']
        lines += ['  }']
        lines += ['}']

        ## Create the SaveStart Method
        lines += ['/** Serialize a task when initially pushing into remote */',
                  'void SaveStart(u32 method, BinaryOutputArchive<true> &ar, Task *task) override {',
                  '  switch (method) {']
        for method_enum_name, method_off in methods:
            if method_off < 0:
                continue
            method_name = method_enum_name.replace('k', '', 1)
            task_name = method_name + "Task"
            lines += [f'    case Method::{method_enum_name}: {{',
                      f'      ar << *reinterpret_cast<{task_name}*>(task);',
                      f'      break;',
                      f'    }}']
        lines += ['  }']
        lines += ['}']

        ## Create the LoadStart Method
        lines += ['/** Deserialize a task when popping from remote queue */',
                  'TaskPointer LoadStart(u32 method, BinaryInputArchive<true> &ar) override {',
                  '  TaskPointer task_ptr;',
                  '  switch (method) {']
        for method_enum_name, method_off in methods:
            if method_off < 0:
                continue
            method_name = method_enum_name.replace('k', '', 1)
            task_name = method_name + "Task"
            lines += [f'    case Method::{method_enum_name}: {{',
                      f'      task_ptr.ptr_ = CHI_CLIENT->NewEmptyTask<{task_name}>(task_ptr.shm_);',
                      f'      ar >> *reinterpret_cast<{task_name}*>(task_ptr.ptr_);',
                      f'      break;',
                      f'    }}']
        lines += ['  }']
        lines += ['  return task_ptr;']
        lines += ['}']

        ## Create the SaveEnd Method
        lines += ['/** Serialize a task when returning from remote queue */',
                  'void SaveEnd(u32 method, BinaryOutputArchive<false> &ar, Task *task) override {',
                  '  switch (method) {']
        for method_enum_name, method_off in methods:
            if method_off < 0:
                continue
            method_name = method_enum_name.replace('k', '', 1)
            task_name = method_name + "Task"
            lines += [f'    case Method::{method_enum_name}: {{',
                      f'      ar << *reinterpret_cast<{task_name}*>(task);',
                      f'      break;',
                      f'    }}']
        lines += ['  }']
        lines += ['}']

        ## Create the LoadEnd Method
        lines += ['/** Deserialize a task when popping from remote queue */',
                  'void LoadEnd(u32 method, BinaryInputArchive<false> &ar, Task *task) override {',
                  '  switch (method) {']
        for method_enum_name, method_off in methods:
            if method_off < 0:
                continue
            method_name = method_enum_name.replace('k', '', 1)
            task_name = method_name + "Task"
            lines += [f'    case Method::{method_enum_name}: {{',
                      f'      ar >> *reinterpret_cast<{task_name}*>(task);',
                      f'      break;',
                      f'    }}']
        lines += ['  }']
        lines += ['}']

        ## Finish the file
        lines += ['', f'#endif  // {METHOD_MACRO}']

        ## Write MOD_NAME_lib_exec.h
        with open(LIB_EXEC_H, 'w') as fp:
            fp.write('\n'.join(lines))

        ## Create the task struct prototypes
        lines = []
        for method_enum_name, method_off in methods:
            if method_off < 0:
                continue
            method_name = method_enum_name.replace('k', '', 1)
            task_name = method_name + "Task"
            lines += [
                self.task_template(task_name, method_enum_name),
                '\n'
            ]

        # Write the new tasks header
        with open(NEW_TASKS_H, 'w') as fp:
            fp.write('\n'.join(lines))

        # Create new client APIs header
        lines = []
        for method_enum_name, method_off in methods:
            if method_off < 0:
                continue
            method_name = method_enum_name.replace('k', '', 1)
            task_name = method_name + "Task"
            lines += [self.client_method_template(task_name, method_name, method_enum_name), '\n']

        # Write the new client header
        with open(NEW_CLIENT_H, 'w') as fp:
            fp.write('\n'.join(lines))

    def task_template(self, task_name, method_enum_name):
        return """
/** The {task_name} task */
struct {task_name} : public Task, TaskFlags<TF_SRL_SYM> {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  {task_name}(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  {task_name}(hipc::Allocator *alloc,
              const TaskNode &task_node,
              const DomainQuery &dom_query,
              const PoolId &state_id,
              const TagId &tag_id,
              const BlobId &blob_id,
              const TagId &tag) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    prio_ = TaskPrio::kLowLatency;
    pool_ = state_id;
    method_ = Method::{method_enum_name};
    task_flags_.SetBits(0);
    dom_query_ = dom_query;

    // Custom
  }

  /** Duplicate message */
  void CopyStart(const {task_name} &other, bool deep) {
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SerializeStart(Ar &ar) {
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(Ar &ar) {
  }
};
""".format(task_name=task_name, method_enum_name=method_enum_name)

    def client_method_template(self, task_name, method_name, method_enum_name):
        return """
/** Metadata task */
void Async{method_name}Construct({task_name} *task,
                    const TaskNode &task_node,
                    const DomainQuery &dom_query) {
  CHI_CLIENT->ConstructTask<{task_name}>(
    task, task_node, dom_query);
}
void {method_name}(const DomainQuery &dom_query) {
  LPointer<MdTask> task =
    AsyncMd(dom_query);
  task->Wait();
  CHI_CLIENT->DelTask(task);
  return;
}
CHI_TASK_METHODS({method_name});
""".format(task_name=task_name,
           method_name=method_name,
           method_enum_name=method_enum_name)

    def server_method_template(self, task_name, method_name, method_enum_name):
        return """

"""
