[![CodeFactor](https://www.codefactor.io/repository/github/kcenon/file_manager/badge)](https://www.codefactor.io/repository/github/kcenon/file_manager)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/448be27b52684c00a3f81d8bdb47bdb3)](https://www.codacy.com/gh/kcenon/file_manager/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=kcenon/file_manager&amp;utm_campaign=Badge_Grade)

## How to use

To understand how to use this library, it provided several sample programs on the samples folder.

1.  [main_server](https://github.com/kcenon/file_manager/tree/main/main_server): the main_server acts for message transferring root. Each middle_server has connected to main_server and all messages have passed through the main_server to each middle_server standardly.
2.  [middle_server](https://github.com/kcenon/file_manager/tree/main/middle_server): the middle_server acts as a gateway between the main_server and each client. For example, send and receive a message packet, and make a file transferring progress packet to send a client. 
3.  [restapi_gateway](https://github.com/kcenon/file_manager/tree/main/restapi_gateway): the restapi_gateway acts for message transferrring to middle_server using RESTAPI.
4.  [download_sample](https://github.com/kcenon/file_manager/tree/main/download_sample): implemented how to use file download via provided micro-server on the micro-services folder
5.  [upload_sample](https://github.com/kcenon/file_manager/tree/main/upload_sample): implemented how to use file upload via provided micro-server on the micro-services folder
6.  [restapi_client_sample](https://github.com/kcenon/file_manager/tree/main/restapi_client_sample): implemented how to use restapi via provided micro-server on the micro-services folder

## License

Note: This license has also been called the "New BSD License" or "Modified BSD License". See also the 2-clause BSD License.

Copyright 2021 🍀☀🌕🌥 🌊

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1.  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2.  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3.  Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## Contact

Please report issues or questions here: https://github.com/kcenon/file_manager/issues
