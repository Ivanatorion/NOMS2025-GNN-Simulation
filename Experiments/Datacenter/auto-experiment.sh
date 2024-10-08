set -xe
pip3 install pyyaml networkx ignnition==1.7.0
autoPath=`realpath .`

mkdir -p results

cd ../..
./waf build

#OBS: LINES 13 - 40 COLLECTS A DATASET AND TRAINS THE GNN MODEL. TO ACHIEVE THE SAME RESULTS FROM OUR PAPER, WE PROVIDE THE WEIGHTS THAT WE USED
#TO TRAIN THE MODEL, UNCOMMENT THOSE LINES AND COMMENT LINE 45

#rcommand="./waf --run-no-build 'FlowGNN --inputFile=$autoPath/datacenter32.txt --outputFilePath=$autoPath/ --genDataset=true'"
#eval $rcommand

#cd $autoPath
#mv skynet.dat ../../gnn/.
#mv queue_graph.dat ../../gnn/.
#rm Throughput.txt
#cd ../../gnn/

#python3 gen_dataset.py
#rm -f data/eval/*
#rm -f data/train/*
#mv skynet.json data/eval/.
#cp data/eval/skynet.json data/train/.
#python3 main.py

#d1=`ls CheckPoint`
#d12=($d1)
#i=${#d12[@]}
#d1=${d12[i-1]}
#d2=`ls CheckPoint/$d1/ckpt/`
#d3=($d2)
#i=${#d3[@]}
#d4=${d3[i-1]}

#python3 view_model.py CheckPoint/$d1/ckpt/$d4

#mv model_weights.dat $autoPath/results/

cd $autoPath

# COMMENT THE LINE BELOW IF YOU WANT TO TRAIN THE MODEL
cp model_weights.dat results/.

mkdir -p results/1-NoECN
mkdir -p results/2-DCTCP
mkdir -p results/3-GNN
mkdir -p results/3-GNN/10ms
mkdir -p results/3-GNN/1ms
mkdir -p results/3-GNN/100us

pi=0

cd ../..

rcommand="./waf --run-no-build 'FlowGNN --inputFile=$autoPath/datacenter32.txt --outputFilePath=$autoPath/results/1-NoECN --enableSwitchEcn=false' &"
eval $rcommand

pids[${pi}]=$!
pi=$((pi+1))

rcommand="./waf --run-no-build 'FlowGNN --inputFile=$autoPath/datacenter32.txt --outputFilePath=$autoPath/results/2-DCTCP --enableSwitchEcn=true' &"
eval $rcommand

pids[${pi}]=$!
pi=$((pi+1))

rcommand="./waf --run-no-build 'FlowGNN --inputFile=$autoPath/datacenter32.txt --outputFilePath=$autoPath/results/3-GNN/10ms --gnnWeightsFile=$autoPath/results/model_weights.dat --gnnUpdatePeriod=10ms' &"
eval $rcommand

pids[${pi}]=$!
pi=$((pi+1))

rcommand="./waf --run-no-build 'FlowGNN --inputFile=$autoPath/datacenter32.txt --outputFilePath=$autoPath/results/3-GNN/1ms --gnnWeightsFile=$autoPath/results/model_weights.dat --gnnUpdatePeriod=1ms' &"
eval $rcommand

pids[${pi}]=$!
pi=$((pi+1))

rcommand="./waf --run-no-build 'FlowGNN --inputFile=$autoPath/datacenter32.txt --outputFilePath=$autoPath/results/3-GNN/100us --gnnWeightsFile=$autoPath/results/model_weights.dat --gnnUpdatePeriod=100us' &"
eval $rcommand

pids[${pi}]=$!
pi=$((pi+1))

cd $autoPath

for pid in ${pids[*]}; do
  wait $pid
done

rm -f results/1-NoECN/queue_graph.dat
rm -f results/2-DCTCP/queue_graph.dat
rm -f results/3-GNN/10ms/queue_graph.dat
rm -f results/3-GNN/1ms/queue_graph.dat
rm -f results/3-GNN/100us/queue_graph.dat

tar czvf results.tar.gz results/

echo "Done!"

