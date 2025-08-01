# RT‑enabled ns‑3 with NVIDIA Sionna

*(fork edition – adds ****SionnaHelper**** & ****SionnaLtePathlossModel****)*

> **This fork keeps full backward‑compatibility with the original **[``](https://github.com/robpegurri/ns3-rt)** while adding C++ code that lets ns‑3 fetch path‑loss directly from Sionna RT.**
>
> | Component                                             | Purpose                                                                                                                                            |
> | ----------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
> | `src/sionna/sionna‑helper.{h,cc}`                     | Singleton that handles UDP messaging (`LOC_UPDATE`, `CALC_REQUEST_*`) with the Python server and offers an optional `SetCarrierFrequency()` API    |
> | `src/spectrum/model/sionna‑lte‑pathloss‑model.{h,cc}` | Sub‑class of `SpectrumPropagationLossModel`; multiplies the Sionna‑returned path‑loss into the Tx PSD inside `DoCalcRxPowerSpectralDensity()`      |
> | `examples/lte/simple‑sionna‑example.cc`               | Demo file; 100 % compatible with the upstream scripts – only TxPower and traffic type are tweaked so you can instantly compare **2 GHz vs 60 GHz** |

---

## ✨ Key Features

- **GPU‑accelerated deterministic ray tracing** – Path‑loss / delay / LoS are computed by NVIDIA Sionna RT for every link.
- **Drop‑in integration** – just set the path‑loss model to `ns3::SionnaLtePathlossModel`; the rest of the LTE stack remains untouched.
- **Run Sionna locally or on a remote GPU server** – default UDP port **8103**.
- **Runtime band switching** – Python side uses `--frequency=…`; C++ side can optionally call `SionnaHelper::SetCarrierFrequency()`.

---

## 🛠️ Installation

Sionna and ns‑3 are two separate processes that talk via UDP. Install them individually.

\### 1 Build ns3‑rt (with the new source files)

```bash
git clone https://github.com/<your‑fork>/ns3-rt.git
cd ns3-rt
./ns3 configure --disable-python --enable-examples
./ns3 build        # waf automatically compiles the added files
```

\### 2 Install Sionna See the [official guide](https://nvlabs.github.io/sionna) for full details; the shortest path is:

```bash
# CPU‑only install
sudo apt install llvm
python3 -m pip install tensorflow sionna

# GPU install (drivers already present)
python3 -m pip install 'tensorflow[and-cuda]' sionna
```

Verify:

```bash
python3 - <<'PY'
import sionna, tensorflow as tf
print(sionna.__version__, tf.config.list_physical_devices())
PY
```

---

## 🚀 Running the Demo

\### 1 Start the Sionna RT server

```bash
python3 sionna_server_script.py \ 
        --local-machine \ 
        --frequency=6e10 \                 # 60 GHz; swap to 2.1 GHz with 2.1e9
        --path-to-xml-scenario=scenarios/SionnaExampleScenario/scene.xml
```

\### 2 Run the ns‑3 simulation

```bash
./ns3 run simple-sionna-example           # default TxPower = 30 dBm
# or override at runtime
./ns3 run "simple-sionna-example --txPower=23"
```

> **Tip** – want the C++ side to know the carrier too?
>
> ```cpp
> SionnaHelper::GetInstance().SetCarrierFrequency(6e10);   // 60 GHz
> ```

---

## 🌐 Running Sionna on a Remote Machine

In your ns‑3 script:

```cpp
SionnaHelper& h = SionnaHelper::GetInstance();
h.SetLocalMachine(false);
h.SetServerIp("YOUR-GPU-SERVER-IP");
```

On the Python side simply omit `--local-machine`. Default port is **UDP 8103**.

---

## 📝 Scene ‑ Node mapping

- Every ns‑3 `Node` is mapped to a mesh in the Sionna scene. The default mesh name is ``.
- If the mesh is missing, Sionna cannot return path‑loss and the sim will error out.
- For scene creation, see NVIDIA's [official video tutorial](https://www.youtube.com/watch?v=7xHLDxUaQ7c).

---

## 📚 Citation

If this project helps your research, please cite:

```bibtex
@misc{pegurri2025digitalnetworktwinsintegrating,
  title        = {Toward Digital Network Twins: Integrating Sionna RT in ns-3 for 6G Multi-RAT Networks Simulations},
  author       = {Roberto Pegurri and Francesco Linsalata and Eugenio Moro and Jakob Hoydis and Umberto Spagnolini},
  year         = {2025},
  eprint       = {2501.00372},
  archivePrefix= {arXiv},
  primaryClass = {cs.NI},
  url          = {https://arxiv.org/abs/2501.00372}
}
```

---

> **Changelog**\
> *2025‑08‑01* — initial fork release: added Helper / PathlossModel documentation.

