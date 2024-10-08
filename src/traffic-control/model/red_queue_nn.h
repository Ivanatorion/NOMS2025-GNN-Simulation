#ifndef RED_QUEUE_DISC_NN_H
#define RED_QUEUE_DISC_NN_H

#include "ns3/queue-disc.h"
#include "ns3/nstime.h"
#include "ns3/boolean.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable-stream.h"

#include <vector>
#include <cmath>

enum Activation {NONE = 0, RELU, SIGMOID};

namespace ns3 {

class RQDNeuralNetworkMatrix{
    public:
        RQDNeuralNetworkMatrix(int lines, int cols) {this->weights.resize(lines); for(int i = 0; i < lines; i++) this->weights[i].resize(cols, 0.0);}

        void SetAt(int i, int j, double value) {if (i < (int) this->weights.size() && j < (int) this->weights[i].size()) this->weights[i][j] = value;}
        double GetAt(int i, int j) {return (i < (int) this->weights.size() && j < (int) this->weights[i].size()) ? this->weights[i][j] : 0.0;}

        int GetNLines(){return (int) this->weights.size();}
        int GetNCols(){return (int) this->weights[0].size();}

        RQDNeuralNetworkMatrix operator * (RQDNeuralNetworkMatrix const &mtx){
            RQDNeuralNetworkMatrix result((int) this->weights.size(), (int) mtx.weights[0].size());
            for(int ir = 0; ir < (int) result.weights.size(); ir++){
                for(int jr = 0; jr < (int) result.weights[ir].size(); jr++){
                    result.weights[ir][jr] = 0.0;
                    for(int k = 0; k < (int) mtx.weights.size(); k++){
                        result.weights[ir][jr] = result.weights[ir][jr] + this->weights[ir][k] * mtx.weights[k][jr];
                    }
                }
            }

            return result;
        }

        void Print(){
            for(int i = 0; i < (int) this->weights.size(); i++){
                for(int j = 0; j < (int) this->weights[i].size(); j++){
                    printf("%.4f ", this->weights[i][j]);
                }
                printf("\n");
            }
        }

    private:
        std::vector<std::vector<double>> weights;
};

class RQDNeuralNetwork{
    public:
        RQDNeuralNetwork(){}

        void AddLayerMatrix(const RQDNeuralNetworkMatrix& m, const RQDNeuralNetworkMatrix& bias, const Activation a) {this->m_layers.push_back(m); this->m_biases.push_back(bias); this->m_activations.push_back(a);}

        RQDNeuralNetworkMatrix Apply(RQDNeuralNetworkMatrix input){
            for(int i = 0; i < (int) this->m_layers.size(); i++){
                input = input * this->m_layers[i];
                for(int j = 0; j < this->m_biases[i].GetNCols(); j++){
                    input.SetAt(0, j, input.GetAt(0, j) + this->m_biases[i].GetAt(0, j));
                    if(this->m_activations[i] == SIGMOID)
                        input.SetAt(0, j, 1.0/(1.0 + exp(-input.GetAt(0, j))));
                    else if(this->m_activations[i] == RELU && input.GetAt(0, j) < 0.0)
                        input.SetAt(0, j, 0.0);
                }
            }
            return input;
        }

        RQDNeuralNetworkMatrix Apply(std::vector<double> input){
            RQDNeuralNetworkMatrix m(1, (int) input.size());
            for(int i = 0; i < (int) input.size(); i++)
                m.SetAt(0, i, input[i]);
            return this->Apply(m);
        }

        int GetNLayers(){return (int) this->m_layers.size();}

    private:
        std::vector<RQDNeuralNetworkMatrix> m_layers;
        std::vector<RQDNeuralNetworkMatrix> m_biases;
        std::vector<Activation> m_activations;
};

}

#endif