#include <node.h>
#include <v8.h>
#include <alsa/asoundlib.h>

using v8::HandleScope;
using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;
using v8::Array;

void fill_control(Isolate *isolate, Local<Object> obj, snd_hctl_elem_t *elem) {
    int err;
    unsigned int count, i;

    snd_ctl_elem_type_t type;
    snd_ctl_elem_info_t *info;
    snd_ctl_elem_value_t *control;

    snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_value_alloca(&control);

    // Get infos
    if ((err = snd_hctl_elem_info(elem, info)) < 0) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "snd_hctl_elem_info error")));
        return;
    }

    // Set device name
    obj->Set(String::NewFromUtf8(isolate, "name"),
        String::NewFromUtf8(isolate, snd_ctl_elem_info_get_name(info)));

    count = snd_ctl_elem_info_get_count(info);

    // Filter by type
    type = snd_ctl_elem_info_get_type(info);
    obj->Set(String::NewFromUtf8(isolate, "type"),
        String::NewFromUtf8(isolate, snd_ctl_elem_type_name(type)));
    switch (type) {
        case SND_CTL_ELEM_TYPE_NONE:
            break;
        case SND_CTL_ELEM_TYPE_BOOLEAN:
            break;
        case SND_CTL_ELEM_TYPE_INTEGER:
            obj->Set(String::NewFromUtf8(isolate, "min"),
                Number::New(isolate, snd_ctl_elem_info_get_min(info)));
            obj->Set(String::NewFromUtf8(isolate, "max"),
                Number::New(isolate, snd_ctl_elem_info_get_max(info)));
            obj->Set(String::NewFromUtf8(isolate, "step"),
                Number::New(isolate, snd_ctl_elem_info_get_step(info)));
            break;
        case SND_CTL_ELEM_TYPE_ENUMERATED:
            obj->Set(String::NewFromUtf8(isolate, "type"),
                String::NewFromUtf8(isolate, "enumerated"));
            break;
        case SND_CTL_ELEM_TYPE_BYTES:
            obj->Set(String::NewFromUtf8(isolate, "type"),
                String::NewFromUtf8(isolate, "bytes"));
            break;
        case SND_CTL_ELEM_TYPE_IEC958:
            obj->Set(String::NewFromUtf8(isolate, "type"),
                String::NewFromUtf8(isolate, "iec958"));
            break;
        case SND_CTL_ELEM_TYPE_INTEGER64:
            obj->Set(String::NewFromUtf8(isolate, "type"),
                String::NewFromUtf8(isolate, "integer64"));
            break;
        default:
            obj->Set(String::NewFromUtf8(isolate, "type"),
                String::NewFromUtf8(isolate, "unknown"));
    }

    // if (snd_ctl_elem_info_is_readable(info)) {
    //     if ((err = snd_hctl_elem_read(elem, control)) < 0) {
    //         isolate->ThrowException(Exception::TypeError(
    //             String::NewFromUtf8(isolate, "snd_hctl_elem_read error")));
    //         return;
    //     }
    //
    //     for (i = 0; i < count; i++) {
    //
    //     }
    // }
}

void list_controls(const v8::FunctionCallbackInfo<v8::Value>& args) {
    int err;
    int i;
    snd_hctl_t *handle;
    snd_hctl_elem_t *elem;
    Isolate* isolate;
    Local<Array> result_list;

    isolate = args.GetIsolate();

    // Why do I have to do that ??
    // TODO: Read https://developers.google.com/v8/embed#handles-and-garbage-collection
    HandleScope scope(isolate);

    // Check input function arguments
    if (args.Length() < 1) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Wrong number of arguments")));
        return;
    }

    if (!args[0]->IsString()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "`Device' argument is not a string")));
        return;
    }

    if ((err = snd_hctl_open(&handle, *String::Utf8Value(args[0]->ToString()), 0)) < 0) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Cannot open device")));
        return;
    }

    if ((err = snd_hctl_load(handle)) < 0) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Cannot load device")));
        return;
    }

    result_list = Array::New(isolate);

    i = 0;
    for (elem = snd_hctl_first_elem(handle); elem; elem = snd_hctl_elem_next(elem)) {
        // Create a new object
        Local<Object> obj = Object::New(isolate);

        fill_control(isolate, obj, elem);

        result_list->Set(i++, obj);
    }

    // Close htcl handle
    snd_hctl_close(handle);

    args.GetReturnValue().Set(result_list);
}

void init(v8::Local<v8::Object> target) {
    NODE_SET_METHOD(target, "list_controls", list_controls);
}

NODE_MODULE(alsactl, init);
