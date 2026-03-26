from pyencoder.train import train_autoencoder
from pyencoder.autoencoder import AutoEncoder
from pyencoder.dataloader import get_data_loader

def main():
    file_path = 'toguz_data.bin' 
    batch_size = 512
    num_epochs = 9999
    learning_rate = 0.005

    data_loader = get_data_loader(file_path, batch_size, num_workers=2)

    autoencoder = AutoEncoder(input_size=902, hidden_size=22)  

    train_autoencoder(autoencoder, data_loader, num_epochs, learning_rate)

if __name__ == "__main__":
    main()