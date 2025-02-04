rm *.csv
rm *.tr

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



today=$(date +"%Y-%m-%d")


for (( NUM_PROCS=5; NUM_PROCS<=5; NUM_PROCS+=32)); do
    for (( EPSILON=10; EPSILON<=40; EPSILON+=10)); do
        for (( INTERVAL=100; INTERVAL<=1000; INTERVAL+=100)); do
            if (( INTERVAL * EPSILON % 1000 == 0 && INTERVAL*EPSILON <= 3000 )); then
                for (( DELTA=1; DELTA<=8; DELTA*=2)); do
                    for (( ALPHA=20; ALPHA<=160; ALPHA*=2)); do
                        MAX_OFFSET_SIZE=$(num_bits $EPSILON)

                        echo "N.${NUM_PROCS}-E.${EPSILON}-I.${INTERVAL}-D.${DELTA}-A.${ALPHA}-M.${MAX_OFFSET_SIZE}"    
                        
                        rm replay-config.h
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

                    done
                done
            fi
        done
    done
done




# touch scratch/$filename