## How to use

To understand how to use this library, it provided several sample programs on the samples folder.

1. [main_server](https://github.com/kcenon/file_manager/tree/main/main_server): the main_server acts for message transferring root. Each middle_server has connected to main_server and all messages have passed through the main_server to each middle_server standardly.
2. [middle_server](https://github.com/kcenon/file_manager/tree/main/middle_server): the middle_server acts as a gateway between the main_server and each client. For example, send and receive a message packet, and make a file transferring progress packet to send a client. 
3. [restapi_gateway](https://github.com/kcenon/file_manager/tree/main/restapi_gateway): the restapi_gateway acts for message transferrring to middle_server using RESTAPI.
4. [download_sample](https://github.com/kcenon/file_manager/tree/main/download_sample): implemented how to use file download via provided micro-server on the micro-services folder
5. [upload_sample](https://github.com/kcenon/file_manager/tree/main/upload_sample): implemented how to use file upload via provided micro-server on the micro-services folder
6. [restapi_client_sample](https://github.com/kcenon/file_manager/tree/main/restapi_client_sample): implemented how to use restapi via provided micro-server on the micro-services folder
