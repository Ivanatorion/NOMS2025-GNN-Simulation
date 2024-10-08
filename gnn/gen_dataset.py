from collections import Counter
from json import dump as json_dump
from networkx import DiGraph
from networkx.readwrite.json_graph import node_link_data
from sys import argv
import yaml

def transform(input_file, graph_file, output_file):
    ecn_lookahead = 5
    
    graphs = []
    graph = DiGraph()
    
    queue_datas = {}
    last_queue_size = {}
    
    print("Lookahead = " + str(ecn_lookahead))
    print("Generating graphs...")
  
    GRAPH_SAMPLE_RATE = 8
    graph_sample_control = 0
  
    with open(input_file) as input_file2:
        glc = 0
        for line in input_file2:
            splited_line = line.split(",")
            
            if(glc == 0 or len(splited_line) == 1):
                assert(glc == 0 and len(splited_line) == 1)
                glc = int(splited_line[0].rstrip())
                
                graph_sample_control = graph_sample_control + 1
                if(graph_sample_control >= GRAPH_SAMPLE_RATE):
                    graph_sample_control = 0
                    graph = DiGraph()
                    hasECN = False
                    for queue in queue_datas.keys():
                        lastQS = 0
                        if queue in last_queue_size.keys():
                            lastQS = last_queue_size[queue]
                        graph.add_node(queue, entity='queue', queue_size=int(queue_datas[queue][0]), last_queue_size=int(queue_datas[queue][0])-lastQS, ecn=int(queue_datas[queue][1]))
                        last_queue_size[queue] = int(queue_datas[queue][0])
                        if int(queue_datas[queue][1]) == 1:
                            hasECN = True
                    if hasECN:
                        with open(graph_file) as graph_lines:
                            splited_graph_lines = (l.split(" ") for l in graph_lines)
                            for sgl in splited_graph_lines:
                                sgl[1] = sgl[1].rstrip()
                                graph.add_edge(sgl[0], sgl[1])
                                graph.add_edge(sgl[1], sgl[0])
                        graphs.append(node_link_data(graph))
            else:
                queue_raw, queue_size_raw, ecn, *_ = splited_line
                if queue_raw in last_queue_size.keys():
                    last_queue_size[queue_raw] = int(queue_datas[queue_raw][0])
                queue_datas[queue_raw] = [int(queue_size_raw), int(ecn)]
                glc = glc - 1
                
    graphs.pop(0)
    print("Total Graphs: " + str(len(graphs)))
    
    for i in range(ecn_lookahead, len(graphs)):
        for n in range(0, len(graphs[i]["nodes"])):
            if graphs[i]["nodes"][n]["ecn"] == 1:
                j = ecn_lookahead
                for n2 in range(0, len(graphs[i - j]["nodes"])):
                    if graphs[i]["nodes"][n]["id"] == graphs[i-j]["nodes"][n2]["id"]:
                        graphs[i-j]["nodes"][n2]["ecn"] = graphs[i]["nodes"][n]["ecn"]
    
    json_dump(graphs, output_file)

def main():
    with open("skynet.json", "w") as output_file:
        transform("skynet.dat", "queue_graph.dat", output_file)

if __name__ == '__main__':
    main()
