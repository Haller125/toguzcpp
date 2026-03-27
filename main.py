import argparse

from pyencoder.train import train_autoencoder
from pyencoder.autoencoder import AutoEncoder
from pyencoder.dataloader import get_data_loader


def parse_args():
    parser = argparse.ArgumentParser(description="Train the autoencoder model")
    parser.add_argument("--file-path", default="toguz_data.bin", help="Path to training data")
    parser.add_argument("--batch-size", type=int, default=512, help="Batch size")
    parser.add_argument("--num-epochs", type=int, default=9999, help="Number of epochs")
    parser.add_argument("--learning-rate", type=float, default=0.005, help="Learning rate")
    parser.add_argument("--num-workers", type=int, default=2, help="Data loader workers")
    parser.add_argument("--input-size", type=int, default=902, help="Autoencoder input size")
    parser.add_argument("--hidden-size", type=int, default=22, help="Autoencoder hidden size")
    return parser.parse_args()


def main():
    args = parse_args()

    data_loader = get_data_loader(args.file_path, args.batch_size, num_workers=args.num_workers)

    autoencoder = AutoEncoder(input_size=args.input_size, hidden_size=args.hidden_size)

    train_autoencoder(autoencoder, data_loader, args.num_epochs, args.learning_rate)

if __name__ == "__main__":
    main()