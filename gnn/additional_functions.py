from keras import backend as K

def loss(y_true, y_pred):
        one_weight = 95.0 / 100.0
        zero_weight = 5.0 / 100.0
        
	# Calculate the binary crossentropy
        b_ce = K.binary_crossentropy(y_true, y_pred)

        # Apply the weights
        weight_vector = y_true * one_weight + (1. - y_true) * zero_weight
        weighted_b_ce = weight_vector * b_ce

        # Return the mean error
        return weighted_b_ce
