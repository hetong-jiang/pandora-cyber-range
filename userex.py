import glob
import archr
import rex

# binary file name
# this should be change lager
filename = "legit_00003"

# 1 -> exploitable
exploitableFlag = 0

# read string that crashes the binary
crashFile = open(glob.glob("/dev/shm/work/fuzzer-master/crashes/id*")[0], 'rb')
crashes = set()
crashes.add(crashFile.read())

# input crash into Rex
# modify filename later
workBinary = archr.targets.LocalTarget([filename], target_os='cgc')
crash = rex.Crash(workBinary, crashes.pop())

# see if exploitable
try:
	crash.explore()
	arsenal = crash.exploit()
	exploitableFlag = 1
except:
	print("Not exploitable")

if exploitableFlag == 1:
	# dump exploit type 1
	try:
		arsenal.best_type1.dump_binary("legit_00003_type1.pov")
		arsenal.best_type1.dump_c("legit_00003_type1.c")
	except:
		print("type 1 not work")

	# dump exploit type 2
	try:
		arsenal.best_type2.dump_binary("legit_00003_type2.pov")
	except:
		print("type 2 not work")
