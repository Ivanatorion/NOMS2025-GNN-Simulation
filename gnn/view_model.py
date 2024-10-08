from collections import Counter
from json import dump as json_dump
from networkx import DiGraph
from networkx.readwrite.json_graph import node_link_data
from sys import argv

from tensorflow import keras
from tensorflow.python.keras.saving import hdf5_format
import h5py
import tensorflow as tf

def loss(y_true, y_pred):
        #return K.sqrt(K.sum(K.square(y_pred*K.cast(y_true>0, "float32") - y_pred), axis=-1) / K.sum(K.cast(y_true>0, "float32") ))
        
        one_weight = 95.0 / 100.0
        zero_weight = 5.0 / 100.0
        
        # Calculate the binary crossentropy
        b_ce = K.binary_crossentropy(y_true, y_pred)
        
        # Apply the weights
        weight_vector = y_true * one_weight + (1. - y_true) * zero_weight
        weighted_b_ce = weight_vector * b_ce
        
        # Return the mean error
        return weighted_b_ce

def main():
    if len(argv) != 2:
        raise ValueError(f"usage: {argv[0]} model_path")

    # Load model
    model = keras.models.load_model(argv[1], custom_objects={'loss': loss})
    
    message_layer_count = 0
    aggregation_layer_count = 0
    update_layer_count = 0
    readout_layer_count = 0
    
    for wg in model.weights:
        if str(wg.name).find("message") >= 0:
            message_layer_count = message_layer_count + 1
        elif str(wg.name).find("aggregation") >= 0:
            aggregation_layer_count = aggregation_layer_count + 1
        elif str(wg.name).find("update") >= 0:
            update_layer_count = update_layer_count + 1
        elif str(wg.name).find("readout") >= 0:
            readout_layer_count = readout_layer_count + 1

    message_layer_count = message_layer_count / 2
    update_layer_count = update_layer_count / 2
    aggregation_layer_count = aggregation_layer_count / 2
    readout_layer_count = readout_layer_count / 2

    with open("model_weights.dat", "w") as out_file:
        out_file.write(str(int(update_layer_count)) + " " + str(int(aggregation_layer_count)) + " " + str(int(message_layer_count)) + " " + str(int(readout_layer_count)) + "\n")
        
        for nn in model.layers:
            for l in nn.layers:
                if(str(l.activation).find("relu") != -1):
                    out_file.write("relu\n")
                elif(str(l.activation).find("sigmoid") != -1):
                    out_file.write("sigmoid\n")
                else:
                    out_file.write("None\n")
        
        for i in range(0, len(model.weights)):
            wg = model.weights[i]
            if i % 2 == 0:
            	out_file.write(str(len(wg.numpy())) + " " + str(len(wg.numpy()[0])) + "\n")
            	for line in wg.numpy():
            	    for w in line:
            	        out_file.write(str(w) + " ")
            	    out_file.write("\n")
            else:
                out_file.write(str(len(wg.numpy())) + "\n")
                for w in wg.numpy():
                    out_file.write(str(w) + " ")
                out_file.write("\n")

if __name__ == '__main__':
    main()
