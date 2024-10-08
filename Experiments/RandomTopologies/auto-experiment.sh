set -xe
pip3 install pyyaml networkx ignnition==1.7.0
autoPath=`realpath .`

mkdir -p results

cd ../..
./waf build

cd $autoPath

for (( j=1; j<=100; j++ ))
do
    mkdir -p results/G$j/1-NoECN
    mkdir -p results/G$j/2-DCTCP
    mkdir -p results/G$j/3-GNN
    mkdir -p results/G$j/3-GNN/10ms
    mkdir -p results/G$j/3-GNN/1ms
    mkdir -p results/G$j/3-GNN/100us
    
    pi=0

    cd ../..

    rcommand="./waf --run-no-build 'FlowGNN --inputFile=$autoPath/Topologies/G_$j.txt --outputFilePath=$autoPath/results/G$j/1-NoECN --enableSwitchEcn=false' &"
    eval $rcommand

    pids[${pi}]=$!
    pi=$((pi+1))

    rcommand="./waf --run-no-build 'FlowGNN --inputFile=$autoPath/Topologies/G_$j.txt --outputFilePath=$autoPath/results/G$j/2-DCTCP --enableSwitchEcn=true' &"
    eval $rcommand

    pids[${pi}]=$!
    pi=$((pi+1))

    rcommand="./waf --run-no-build 'FlowGNN --inputFile=$autoPath/Topologies/G_$j.txt --outputFilePath=$autoPath/results/G$j/3-GNN/10ms --gnnWeightsFile=$autoPath/model_weights.dat --gnnUpdatePeriod=10ms' &"
    eval $rcommand

    pids[${pi}]=$!
    pi=$((pi+1))

    rcommand="./waf --run-no-build 'FlowGNN --inputFile=$autoPath/Topologies/G_$j.txt --outputFilePath=$autoPath/results/G$j/3-GNN/1ms --gnnWeightsFile=$autoPath/model_weights.dat --gnnUpdatePeriod=1ms' &"
    eval $rcommand

    pids[${pi}]=$!
    pi=$((pi+1))

    rcommand="./waf --run-no-build 'FlowGNN --inputFile=$autoPath/Topologies/G_$j.txt --outputFilePath=$autoPath/results/G$j/3-GNN/100us --gnnWeightsFile=$autoPath/model_weights.dat --gnnUpdatePeriod=100us' &"
    eval $rcommand

    pids[${pi}]=$!
    pi=$((pi+1))

    cd $autoPath

    for pid in ${pids[*]}; do
      wait $pid
    done

    rm -f results/G$j/1-NoECN/queue_graph.dat
    rm -f results/G$j/2-DCTCP/queue_graph.dat
    rm -f results/G$j/3-GNN/10ms/queue_graph.dat
    rm -f results/G$j/3-GNN/1ms/queue_graph.dat
    rm -f results/G$j/3-GNN/100us/queue_graph.dat
done

tar czvf results.tar.gz results/

echo "Done!"

