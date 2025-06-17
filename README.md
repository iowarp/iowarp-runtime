# Chimaera: The Core Runtime of the IOWarp Project

<p align="center">
  <strong>The high-performance, modular runtime for IOWarp's near-data processing and data storage systems.</strong>
  <br />
  <br />
  <a href="#getting-started">Getting Started</a> ·
  <a href="#developing-a-chimod">Developing ChiMods</a> ·
  <a href="#contributing">Contribute</a> ·
  <a href="#acknowledgements">Acknowledgements</a>
</p>

---

**Chimaera** is the core runtime engine for the **[IOWarp](https://grc.iit.edu/research/projects/iowarp)** project. It is a distributed semi-microkernel designed from the ground up to enable high-performance, modular, and scalable systems for scientific workflows. It empowers developers to build complex applications for near-data processing, where computation happens as close to the data as possible, minimizing data movement and maximizing performance.

As a key component of IOWarp, Chimaera provides the foundational building blocks for custom storage engines, distributed file systems, and accelerated data analytics pipelines.

## Key Features

*   🚀 **High Performance:** Built for low-latency and high-throughput I/O. Communication is optimized through shared memory for local inter-process communication and high-speed networks for distributed nodes.
*   🧩 **Extreme Modularity (ChiMods):** Chimaera's core is minimal. All application-level logic is implemented in dynamically loaded modules we call **"ChiMods"**. This allows you to extend and customize the runtime for your specific needs without modifying the core.
*   🌐 **Scalable & Distributed:** Designed to scale from a single node to a large cluster, Chimaera handles the complexity of distributed communication and resource management.
*   🔧 **Extensible by Design:** The modular architecture makes it easy to add new storage backends, networking protocols, or custom data processing tasks.

## Architecture

Chimaera operates on a semi-microkernel design. A central **Runtime** process runs on each node, managing core resources. Specialized services are provided by **ChiMods**, which are dynamically loaded by the runtime. Client applications interact with the system through a lightweight client library.

This modular design keeps the core clean and allows for immense flexibility.

## Getting Started

Ready to dive in? Here's how to get Chimaera up and running on your system.

### 1. Prerequisites

The build system relies on a custom tool called `scspkg` and standard tools like CMake, MPI, and a C++17 compiler.

*   **System Dependencies:** `cmake`, `mpi`, `gcc >= 9`
*   **Python Dependencies:**
    ```bash
    pip install -r requirements.txt
    ```
*   **`scspkg`:** The build process uses a tool called `scspkg`. Please ensure you have it installed and configured. *(Note: We are working to simplify this process).*

### 2. Clone the Repository

```bash
git clone https://github.com/iowarp/iowarp-runtime.git
cd iowarp-runtime
```

### 3. Build Chimaera

The build is a three-step process:

```bash
# 1. Configure the build environment using scspkg
sh env.sh

# 2. Generate the build files with CMake
cmake -B build .

# 3. Compile the source code
make -C build -j $(nproc)
```

### 4. Run the Runtime & An Example

First, start the Chimaera runtime daemon on a terminal:
```bash
./build/bin/chimaera_start_runtime
```
The runtime will require a configuration file. Default configurations can be found in the `/config` directory.

Next, open another terminal and run a benchmark to test a `small_message` ChiMod:
```bash
# Usage: ./build/bin/bench_chimaera_latency <depth> <ops> <async (0 or 1)>
./build/bin/bench_chimaera_latency 0 1000 0
```
You should see output from the benchmark, indicating that your client application successfully communicated with the Chimaera runtime!

## Developing a ChiMod

The true power of Chimaera is unlocked by creating your own **ChiMods**. A ChiMod is a self-contained unit of functionality that can be loaded by the runtime. The `tasks/` directory contains many examples.

To create a new module named `my_mod`, you would typically create a directory `tasks/my_mod` with the following structure:

*   `chimaera_mod.yaml`: Defines the metadata for your module.
*   `include/my_mod/my_mod_methods.yaml`: The public API for your module, defining the methods that clients can call.
*   `include/my_mod/my_mod_client.h`: The header for the client-side library.
*   `include/my_mod/my_mod_tasks.h`: Header for the runtime-side task implementation.
*   `src/my_mod_client.cc`: The implementation of the client library.
*   `src/my_mod_runtime.cc`: The server-side implementation of your module's logic.

The existing ChiMods in the `tasks` directory serve as excellent reference implementations.

## Contributing

We are excited to grow the Chimaera community and welcome contributions! Whether it's a bug fix, a new feature, or a new ChiMod, we appreciate your help.

### Reporting Issues

*   Please report bugs, feature requests, or questions by opening an **Issue** on GitHub.
*   For bugs, please include:
    *   The steps to reproduce the issue.
    *   The expected behavior.
    *   The actual behavior and any relevant logs.
    *   Your system configuration (OS, compiler, etc.).

### Pull Requests

We use a standard fork-and-pull-request workflow.

1.  **Fork** the repository to your own GitHub account.
2.  **Clone** your fork locally: `git clone https://github.com/YOUR_USERNAME/iowarp-runtime.git`
3.  Create a **new branch** for your changes: `git checkout -b my-awesome-feature`
4.  Make your changes and **commit** them with clear, descriptive messages.
5.  **Push** your branch to your fork: `git push origin my-awesome-feature`
6.  Open a **Pull Request** from your branch to the `main` branch of the original repository.

### Coding Style

We use `cpplint` to maintain a consistent C++ coding style. Please run the linter before submitting a pull request. The configuration can be found in `CPPLINT.cfg`.

```bash
# Run from the root of the repository
sh ci/lint.sh .
```

## License

Chimaera is licensed under the **BSD 3-Clause License**. You can find the full license text in the source files.

---

## Acknowledgements

Chimaera is a core component of the IOWarp project, which is developed at the [Gnosis Research Center (GRC)](https://grc.iit.edu/) at the Illinois Institute of Technology.

This project is funded by the National Science Foundation (NSF) and is designed to support next-generation scientific workflows. We are grateful for the support from our partners and the open-source community. For more details on the IOWarp project, please visit the [IOWarp project page](https://grc.iit.edu/research/projects/iowarp).
