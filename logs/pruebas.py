import re, sys, os, argparse
import pandas as pd

LOG_DIR_DEFAULT = "logs"
CSV_OUT = "logs/results_suiteA.csv"
CSV_SPEEDUP = "logs/suiteA_speedup_eficiencia.csv"

# Regex para nombres de archivo:
# seq_W1024_H768_N16_run01.log
#  - omp_W1024_H768_N50_run01.log               (sin número de hilos)
#  - omp8_W1024_H768_N50_run01.log              (con número de hilos)
NAME_RE = re.compile(
    r"^(?:(seq)|omp(?:_(\d+))?)_W(\d+)_H(\d+)_N(\d+)_run(\d+)\.log$"
)

SIM_PAT   = re.compile(r'(?:sim(?:\+ink)?(?:\([^)]+\))?)=\s*([0-9]*\.?[0-9]+)')
SHADE_PAT = re.compile(r'(?:shade\+present(?:\([^)]+\))?)=\s*([0-9]*\.?[0-9]+)')
FPS_PAT   = re.compile(r'FPS\s*[:=]\s*([0-9]*\.?[0-9]+)')

def parse_log_file(path):
    txt = open(path, "r", encoding="utf-8", errors="ignore").read()
    def mean_from(pat):
        vals = [float(x) for x in pat.findall(txt)]
        return (sum(vals)/len(vals)) if vals else float("nan")
    sim  = mean_from(SIM_PAT)
    shd  = mean_from(SHADE_PAT)
    fps  = mean_from(FPS_PAT)
    return sim, shd, fps

def scan_logs(log_dir):
    rows = []
    for fn in os.listdir(log_dir):
        if not fn.endswith(".log"):
            continue
        m = NAME_RE.match(fn)
        if not m:
            # Ignora otros logs
            continue
        seq_tag, p_str, W, H, N, run = m.groups()
        mode = "seq" if seq_tag else "omp"
        p = 1 if mode=="seq" else (int(p_str) if p_str else 0)
        sim, shd, fps = parse_log_file(os.path.join(log_dir, fn))
        rows.append({
            "suite": "A",
            "N": int(N),
            "W": int(W),
            "H": int(H),
            "mode": mode,
            "p": int(p),
            "run": int(run),
            "mean_sim_ms": sim,
            "mean_shade_ms": shd,
            "mean_FPS": fps
        })
    return pd.DataFrame(rows)

def compute_speedup(df):
    """
    Devuelve una tabla por (N, p) con:
      - Tseq_ms:  tiempo medio de simulación secuencial
      - Tpar_ms:  tiempo medio de simulación paralelo (p hilos)
      - Speedup:  Tseq_ms / Tpar_ms
      - Eficiencia: Speedup / p
      - FPS_seq:  FPS medio en secuencial (baseline por N)
      - FPS_omp:  FPS medio en paralelo (por N, p)
    """
    # Promedios por configuración (N, mode, p)
    g = df.groupby(["N", "mode", "p"], as_index=False).agg(
        mean_sim_ms=("mean_sim_ms", "mean"),
        mean_FPS=("mean_FPS", "mean"),
    )

    base = g[g["mode"] == "seq"][["N", "mean_sim_ms", "mean_FPS"]].rename(
        columns={"mean_sim_ms": "Tseq_ms", "mean_FPS": "FPS_seq"}
    )
    omp = g[g["mode"] == "omp"][["N", "p", "mean_sim_ms", "mean_FPS"]].rename(
        columns={"mean_sim_ms": "Tpar_ms", "mean_FPS": "FPS_omp"}
    )

    if omp.empty:
        print("⚠️  No se encontraron corridas paralelas (mode=omp).")
        return pd.DataFrame()

    rows = []
    for _, r in omp.iterrows():
        N = int(r["N"])
        p = int(r["p"])
        Tpar = float(r["Tpar_ms"])
        FPS_omp = float(r["FPS_omp"])

        base_row = base[base["N"] == N]
        if base_row.empty:
            continue

        # usar .iloc[0] para evitar FutureWarning
        Tseq = float(base_row["Tseq_ms"].iloc[0])
        FPS_seq = float(base_row["FPS_seq"].iloc[0])

        S = Tseq / Tpar if Tpar > 0 else float("nan")
        E = S / p if p > 0 else float("nan")

        rows.append({
            "N": N,
            "p": p,
            "Tseq_ms": Tseq,
            "Tpar_ms": Tpar,
            "Speedup": S,
            "Eficiencia": E,
            "FPS_seq": FPS_seq,
            "FPS_omp": FPS_omp,
        })

    res = pd.DataFrame(rows)
    return res.sort_values(["N", "p"]) if not res.empty else res


def main():
    ap = argparse.ArgumentParser(description="Suite A – Procesa logs y calcula speedup/eficiencia por N.")
    ap.add_argument("--logs", default=LOG_DIR_DEFAULT, help="Carpeta con archivos .log (por defecto: logs)")
    ap.add_argument("--csv", default=CSV_OUT, help="Ruta para results_suiteA.csv")
    ap.add_argument("--speedup_csv", default=CSV_SPEEDUP, help="Ruta para suiteA_speedup_eficiencia.csv")
    ap.add_argument("--threads", type=int, default=0, help="Número de hilos OMP a usar cuando los logs 'omp_' no incluyen el número en el nombre (default: 0 = desconocido).")
    args = ap.parse_args()

    if not os.path.isdir(args.logs):
        sys.exit(f"Carpeta de logs no encontrada: {args.logs}")

    df = scan_logs(args.logs)
    if df.empty:
        sys.exit("No se encontraron logs válidos (nombres esperados: seq_W... o ompP_W...).")

    if args.threads > 0:
        df.loc[(df["mode"]=="omp") & (df["p"]==0), "p"] = args.threads

    # Guarda resultados por corrida
    df_sorted = df.sort_values(["mode","N","p","run"])
    df_sorted.to_csv(args.csv, index=False)
    print(f"✅ Escrito {args.csv} con {len(df_sorted)} filas.")
    print(df_sorted.to_string(index=False))

    # Calcula speedup/eficiencia por N (si hay OMP)
    speed = compute_speedup(df_sorted)
    if speed.empty:
        print("⚠️  No se generó tabla de speedup (faltan corridas OMP).")
    else:
        speed.to_csv(args.speedup_csv, index=False)
        print(f"\n✅ Escrito {args.speedup_csv}:\n")
        print(speed.to_string(index=False, float_format=lambda x: f"{x:.3f}"))

if __name__ == "__main__":
    main()


"""
python logs/pruebas.py --logs logs --threads 8

Para probar secuencial

W=1024; H=768; SEED=42; DURATION=15
for N in 50 100 200; do
  for i in $(seq -w 1 10); do
    timeout ${DURATION}s ./build/screensaver \
      --width $W --height $H --N $N --seed $SEED \
      --novsync --profile --fpslog \
      > "logs/seq_W${W}_H${H}_N${N}_run${i}.log"
  done
done

"""

"""
Para probar paralela

P=$(nproc)   # o P=8
W=1024; H=768; SEED=42; DURATION=15
for N in 50 100 200; do
  for i in $(seq -w 1 10); do
    OMP_NUM_THREADS=$P timeout ${DURATION}s ./build/screensaver_parallel \
      --width $W --height $H --N $N --seed $SEED \
      --novsync --profile --fpslog \
      > "logs/omp${P}_W${W}_H${H}_N${N}_run${i}.log"
  done
done


"""
