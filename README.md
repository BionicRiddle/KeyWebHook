# KeyWebHook

### Features:
- Allows users to define custom keyboard shortcuts to trigger HTTP requests.
- Supports multiple shortcuts with customizable URLs, request methods, headers.

### Usage:
1. Rename `config.json.example` into `config.json`
2. Edit the `config.json` file to define your shortcuts, define the following properties:
   - **keys:** The combination of keys to trigger the shortcut. You can use virtual key codes, represented in hexadecimal format. For example, `0x10+0x11+0x12+0x31` corresponds to Shift + Ctrl + Alt + 1.
   - **url:** The target URL for the HTTP request.
   - **method:** The HTTP method to use (e.g., `GET` or `POST`).
   - **headers:** (Optional) Additional headers to include in the HTTP request. It should be specified as a JSON object. For example see: [`config.json.example`](config.json.example)
   - **alertOnError:** (Optional) If set to true, the application will display an alert if the HTTP request encounters an error.

3. Run the KeyWebHook executable.
4. The application runs in the system tray, waiting for defined keyboard shortcuts to trigger actions.

### Virtual Key Codes:
Virtual key codes can be found [here](https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes).

### Disclaimer:
- Use this software at your own risk. The developer is not responsible for any misuse or damage caused by the application.
