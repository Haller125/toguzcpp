import torch
from torch import nn, optim


def train_autoencoder(autoencoder, data_loader, num_epochs, initial_lr, patience=5):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    autoencoder = autoencoder.to(device)

    pos_weight = torch.tensor([90.0], device=device) # Example weight to favor '1's
    criterion = nn.BCEWithLogitsLoss(pos_weight=pos_weight)
    
    optimizer = optim.Adam(autoencoder.parameters(), lr=initial_lr)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, mode='min', factor=0.996, patience=2)

    best_loss = float('inf')
    early_stop_counter = 0

    for epoch in range(num_epochs):
        epoch_loss = 0
        autoencoder.train()
        
        for data in data_loader:
            data = data.to(device, non_blocking=True).float()

            # Forward pass
            reconstructed = autoencoder(data)
            loss = criterion(reconstructed, data)

            # Backward pass
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()
            
            epoch_loss += loss.item()

        avg_loss = epoch_loss / len(data_loader)
        
        # 3. Update Scheduler
        scheduler.step(avg_loss)
        
        print(f'Epoch [{epoch+1}/{num_epochs}], Avg Loss: {avg_loss:.4f}, LR: {optimizer.param_groups[0]["lr"]}')

        # 4. Early Stopping Logic
        if avg_loss < best_loss:
            best_loss = avg_loss
            early_stop_counter = 0
        else:
            early_stop_counter += 1
            if early_stop_counter >= patience:
                print(f"Early stopping triggered at epoch {epoch+1}")
                break