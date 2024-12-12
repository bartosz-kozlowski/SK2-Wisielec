import socket
import tkinter as tk
from tkinter import scrolledtext
import selectors
import threading

BUFFER_SIZE = 1024

# Create a selector for non-blocking IO
selector = selectors.DefaultSelector()

# Global variables
client_socket = None
nickname = ""
chat_box = None
message_entry = None
nickname_entry = None
nickname_button = None
send_button = None

def send_message():
    global client_socket, message_entry
    message = message_entry.get()
    if message and client_socket:
        client_socket.send(message.encode())
        message_entry.delete(0, tk.END)

def set_nickname():
    global nickname, nickname_entry, chat_box, nickname_button, send_button
    nickname = nickname_entry.get()
    if nickname:
        nickname_entry.delete(0, tk.END)
        chat_box.config(state=tk.NORMAL)
        chat_box.insert(tk.END, f"Nickname set: {nickname}\n")
        chat_box.config(state=tk.DISABLED)
        nickname_button.config(state=tk.DISABLED)
        nickname_entry.config(state=tk.DISABLED)
        start_client()

def start_client():
    global client_socket
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.connect(('127.0.0.1', 12345))
    client_socket.setblocking(False)

    # Register the socket for reading
    selector.register(client_socket, selectors.EVENT_READ, receive_message)

    # Send the nickname to the server
    client_socket.send(nickname.encode())

    # Start a thread for the event loop
    threading.Thread(target=run_event_loop, daemon=True).start()

def receive_message(client_socket):
    try:
        data = client_socket.recv(BUFFER_SIZE)
        if data:
            message = data.decode()
            chat_box.config(state=tk.NORMAL)
            chat_box.insert(tk.END, f"{message}\n")
            chat_box.config(state=tk.DISABLED)
        else:
            # Server closed the connection
            selector.unregister(client_socket)
            client_socket.close()
    except Exception as e:
        print(f"Error receiving message: {e}")

def run_event_loop():
    while True:
        events = selector.select()
        for key, _ in events:
            callback = key.data
            callback(key.fileobj)

# GUI setup
def setup_gui(root):
    global chat_box, message_entry, nickname_entry, nickname_button, send_button

    chat_box = scrolledtext.ScrolledText(root, wrap=tk.WORD, state=tk.DISABLED)
    chat_box.grid(row=0, column=0, padx=10, pady=10)

    message_entry = tk.Entry(root, width=50)
    message_entry.grid(row=1, column=0, padx=10, pady=10)

    send_button = tk.Button(root, text="Send", command=send_message)
    send_button.grid(row=1, column=1, padx=10, pady=10)

    nickname_entry = tk.Entry(root, width=50)
    nickname_entry.grid(row=2, column=0, padx=10, pady=10)

    nickname_button = tk.Button(root, text="Set Nickname", command=set_nickname)
    nickname_button.grid(row=2, column=1, padx=10, pady=10)

# Main function to create the GUI and run the app
def main():
    root = tk.Tk()
    root.title("Chat Client")
    setup_gui(root)
    root.mainloop()

if __name__ == "__main__":
    main()
