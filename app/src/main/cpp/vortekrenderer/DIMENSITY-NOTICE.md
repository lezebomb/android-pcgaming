# Vortek Android server port

Imported from brunodev85/winlator app submodule commit
`a030f552f452158a2db64fdb32b490fa19c0b48d`, paired with Vortek client
`b1730c5def9b575672e671aee11d79ae7adc63d1` (LGPL-2.1; see LICENSE).
The serializer must match the pinned client. Vulkan headers are the paired
Khronos 1.3.302 headers and retain their original notices.

Local changes: JNI namespace for this app; system Mali Vulkan only; checked
initialization; cache directory supplied by Android Context; bounded cache
paths; error cleanup during AHB image creation. This backend is experimental
and does not establish game compatibility. No system driver is installed.
