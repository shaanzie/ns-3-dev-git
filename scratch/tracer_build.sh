rm *.csv
rm *.tr

NUM_PROCS=5
EPSILON=100
INTERVAL=100
DELTA=200
ALPHA=20


# Function to calculate the number of bits required to store an integer
num_bits() {
    number=$1
    bits=0
    while ((number > 0)); do
        ((bits++))
        number=$((number >> 1))
    done
    echo $bits
}

MAX_OFFSET_SIZE=$(num_bits $EPSILON)

today=$(date +"%Y-%m-%d")

echo "N.${NUM_PROCS}-E.${EPSILON}-I.${INTERVAL}-D.${DELTA}-A.${ALPHA}-M.${MAX_OFFSET_SIZE}"    
                        
touch replay-config.h

echo "#define REPCL_CONFIG_H" >> replay-config.h

echo "#define NUM_PROCS $NUM_PROCS" >> replay-config.h

echo "#define EPSILON $EPSILON" >> replay-config.h
echo "#define INTERVAL $INTERVAL" >> replay-config.h
echo "#define ALPHA $ALPHA" >> replay-config.h
echo "#define DELTA $DELTA" >> replay-config.h

echo "#define MAX_OFFSET_SIZE $MAX_OFFSET_SIZE" >> replay-config.h

rm src/config-store/model/replay-config.h
mv replay-config.h src/config-store/model/

./ns3 run scratch/replay-simulator.cc > results-${today}-N.${NUM_PROCS}-E.${EPSILON}-I.${INTERVAL}-D.${DELTA}-A.${ALPHA}-M.${MAX_OFFSET_SIZE}.csv 2>&1

cd ..

cd causal-traces-repcl/src

python3 ../causal-traces-repcl/src/run_tracing.py results-${today}-N.${NUM_PROCS}-E.${EPSILON}-I.${INTERVAL}-D.${DELTA}-A.${ALPHA}-M.${MAX_OFFSET_SIZE}.csv

python3 ../causal-traces-repcl/src/dash_app.py generated_trace.json