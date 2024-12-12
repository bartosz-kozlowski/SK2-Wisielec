import socket
import tkinter as tk
from tkinter import scrolledtext
import selectors
import threading

BUFFER_SIZE = 1024

# Create a selector for non-blocking IO
selector = selectors.DefaultSelector()

class ChatClient:
    def __init__(self, root, host='127.0.0.1', port=12345):
        self.root = root
        self.host = host
        self.port = port
        self.client_socket = None
        self.nickname = ""
        
        # GUI components
        self.chat_box = scrolledtext.ScrolledText(root, wrap=tk.WORD, state=tk.DISABLED)
        self.chat_box.grid(row=0, column=0, padx=10, pady=10)
        
        self.message_entry = tk.Entry(root, width=50)
        self.message_entry.grid(row=1, column=0, padx=10, pady=10)
        
        self.send_button = tk.Button(root, text="Send", command=self.send_message)
        self.send_button.grid(row=1, column=1, padx=10, pady=10)

        self.nickname_entry = tk.Entry(root, width=50)
        self.nickname_entry.grid(row=2, column=0, padx=10, pady=10)
        
        self.nickname_button = tk.Button(root, text="Set Nickname", command=self.set_nickname)
        self.nickname_button.grid(row=2, column=1, padx=10, pady=10)

    def send_message(self):
        message = self.message_entry.get()
        if message and self.client_socket:
            self.client_socket.send(message.encode())
            self.message_entry.delete(0, tk.END)
        
    def set_nickname(self):
        self.nickname = self.nickname_entry.get()
        if self.nickname:
            self.nickname_entry.delete(0, tk.END)
            self.chat_box.config(state=tk.NORMAL)
            self.chat_box.insert(tk.END, f"Nickname set: {self.nickname}\n")
            self.chat_box.config(state=tk.DISABLED)
            self.nickname_button.config(state=tk.DISABLED)
            self.nickname_entry.config(state=tk.DISABLED)
            self.start_client()

    def start_client(self):
        self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.client_socket.connect((self.host, self.port))
        self.client_socket.setblocking(False)
        
        # Register the socket for reading
        selector.register(self.client_socket, selectors.EVENT_READ, self.receive_message)
        
        # Send the nickname to the server
        self.client_socket.send(self.nickname.encode())
        
        # Start a thread for the event loop
        threading.Thread(target=self.run_event_loop, daemon=True).start()

    def receive_message(self, client_socket):
        try:
            data = client_socket.recv(BUFFER_SIZE)
            if data:
                message = data.decode()
                self.chat_box.config(state=tk.NORMAL)
                self.chat_box.insert(tk.END, f"{message}\n")
                self.chat_box.config(state=tk.DISABLED)
            else:
                # Server closed the connection
                selector.unregister(client_socket)
                client_socket.close()
        except Exception as e:
            print(f"Error receiving message: {e}")
        
    def run_event_loop(self):
        while True:
            events = selector.select()
            for key, _ in events:
                callback = key.data
                callback(key.fileobj)

# Create and run the GUI
def main():
    root = tk.Tk()
    root.title("Chat Client")
    app = ChatClient(root)
    root.mainloop()

if __name__ == "__main__":
    main()
