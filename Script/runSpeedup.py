# Run Speed Up Data(Prepare Text File)
import os
from extractTextFile import OutputExtractor

# 首先從 Input 拿到不要執行的 Phase
notExecute = list(map(lambda x: int(x), input("Put the Phase that u not want to execute.(seperate by space) ").split(" ")))

OUTPUTDIR = "logs"
DATASET = {
    "index": "35",
    "n": "536869888",
    "fIn": "35.in",
    "fOut": "35.out"
}
# DATASET = {
#     "index": "27",
#     "n": "191121",
#     "fIn": "27.in",
#     "fOut": "27.out"
# }
SEQUENCIALVIERSION = f"{DATASET['index']}-Sequencial.txt"

def getOutputPath(fileName: str):
    return f"{OUTPUTDIR}/{fileName}"

def getSingleCoreFileName(i: int):
    return DATASET["index"] + f"-SingleCore-{i}.txt"

def getSingleCoreFileNameFull(i: int):
    return DATASET["index"] + f"-SingleCore-{i}-Full.txt"

def getMultipleCoreFileName(i: int):
    return DATASET["index"] + f"-MultiCore-{i}.txt"

def getMultipleCoreFileNameFull(i: int):
    return DATASET["index"] + f"-MultiCore-{i}-Full.txt"

print("Program Start.......")
if (not os.path.isdir(f"./{OUTPUTDIR}")):
    os.mkdir(OUTPUTDIR)
    print(f"{OUTPUTDIR} created.")

# Get Single Nodes Performance
# os.system(f"./seq {DATASET['n']} {DATASET['fIn']} {DATASET['fOut']} > {getOutputPath(SEQUENCIALVIERSION)}")
# print("Sequencial Version Completed.")

if 0 not in notExecute:
    # 12 Cores perNode
    for i in range(12):
        os.system(f"srun -N1 -n{i+1} ./alltime {DATASET['n']} {DATASET['fIn']} {DATASET['fOut']} > {getOutputPath(getSingleCoreFileName(i))}")
        print(f"{i+1} cores completed.")
    print("----------------------------------------------------------")

if 1 not in notExecute:
    # 8 Process PerNode
    for i in range(4):
        os.system(f"srun -N{i+1} -n{(i+1)*8} ./alltime {DATASET['n']} {DATASET['fIn']} {DATASET['fOut']} > {getOutputPath(getMultipleCoreFileName(i))}")
        print(f"{i+1} multi-cores completed.")
    print("----------------------------------------------------------")

if 2 not in notExecute:
    # 12 Cores perNode but Full Information
    for i in range(12):
        os.system(f"srun -N1 -n{i+1} ./time {DATASET['n']} {DATASET['fIn']} {DATASET['fOut']} > {getOutputPath(getSingleCoreFileNameFull(i))}")
        print(f"{i+1} cores full completed.")
    print("----------------------------------------------------------")

if 3 not in notExecute:
    # 8 Process PerNode
    for i in range(4):
        os.system(f"srun -N{i+1} -n{(i+1)*8} ./time {DATASET['n']} {DATASET['fIn']} {DATASET['fOut']} > {getOutputPath(getMultipleCoreFileNameFull(i))}")
        print(f"{i+1} multi-cores Full completed.")
    print("----------------------------------------------------------")