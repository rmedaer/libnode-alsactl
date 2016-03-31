#include <node.h>
#include <v8.h>
#include <alsa/asoundlib.h>

#define ALSACTL_SUCCESS 0

using v8::HandleScope;
using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Number;
using v8::Boolean;
using v8::Object;
using v8::String;
using v8::Value;
using v8::Array;
using v8::Null;


Local<Array> alsactl_fill_control_items(
    Isolate *isolate,
    snd_hctl_elem_t *elem,
    snd_ctl_elem_info_t *info)
{
    Local<Array> items;
    unsigned int index, count;

    items = Array::New(isolate);

    count = snd_ctl_elem_info_get_items(info);
    for (index = 0; index < count; index++) {
        snd_ctl_elem_info_set_item(info, index);
        if (snd_hctl_elem_info(elem, info) < 0) {
            items->Set(index, Null(isolate));
        } else {
            items->Set(index, String::NewFromUtf8(isolate, snd_ctl_elem_info_get_item_name(info)));
        }
    }

    return items;
}

Local<Value> alsactl_fill_control_value(
    Isolate *isolate,
    snd_ctl_elem_type_t type,
    snd_ctl_elem_value_t *value,
    unsigned index)
{
    switch (type) {
        case SND_CTL_ELEM_TYPE_NONE:
            break;
        case SND_CTL_ELEM_TYPE_BOOLEAN:
            return Boolean::New(isolate, snd_ctl_elem_value_get_boolean(value, index));
        case SND_CTL_ELEM_TYPE_INTEGER:
            return Number::New(isolate, snd_ctl_elem_value_get_integer(value, index));
        case SND_CTL_ELEM_TYPE_ENUMERATED:
            return Number::New(isolate, snd_ctl_elem_value_get_enumerated(value, index));   //snd_ctl_elem_info_get_item_name
            break;
        case SND_CTL_ELEM_TYPE_BYTES:
            break;
        case SND_CTL_ELEM_TYPE_IEC958:
            break;
        case SND_CTL_ELEM_TYPE_INTEGER64:
            return Number::New(isolate, snd_ctl_elem_value_get_integer64(value, index));
        default:
            // TODO return ::NULL
            break;
    }

    return Null(isolate);
}

Local<Value> alsactl_wrap_control_value(snd_hctl_elem_t *elem) {
    int err;
    unsigned count, index;
    snd_ctl_elem_type_t type;
    snd_ctl_elem_info_t *info;
    snd_ctl_elem_value_t *value;
    Isolate *isolate;

    // Allocate memory for current control's info and value
    snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_value_alloca(&value);

    // Retrieve scope
    if (!(isolate = Isolate::GetCurrent())) {
        isolate = Isolate::New();
        isolate->Enter();
    }

    // Get control's infos
    if ((err = snd_hctl_elem_info(elem, info)) < 0) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "snd_hctl_elem_info error"))); // TODO refactor error
        return Null(isolate);
    }

    // Number of values
    count = snd_ctl_elem_info_get_count(info);

    if ((err = snd_hctl_elem_read(elem, value)) < 0) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "snd_hctl_elem_read error")));
        return Null(isolate);
    }

    // Type
    type = snd_ctl_elem_info_get_type(info);

    if (count <= 1) {
        return alsactl_fill_control_value(isolate, type, value, 0);
    } else if (count > 1) {
        Local<Array> values = Array::New(isolate);

        for (index = 0; index < count; index++) {
            values->Set(index, alsactl_fill_control_value(isolate, type, value, index));
        }

        return values;
    }

    // Ignore the following line, it's a fake for compiler
    // TODO: find a way to ignore it
    return Null(isolate);
}

/**
 * Export an control from an asound element to a given Node object.
 * @param {Local<Object>} obj - Node object to fill
 * @param {snd_hctl_elem_t} elem - Asound element.
 * @return {Local<Value>} - v8 object or Null
 */
Local<Value> alsactl_wrap_control(snd_hctl_elem_t *elem) {
    int err;
    Isolate *isolate;
    snd_ctl_elem_type_t type;
    snd_ctl_elem_info_t *info;
    Local<Object> obj;

    // Retrieve scope
    if (!(isolate = Isolate::GetCurrent())) {
        isolate = Isolate::New();
        isolate->Enter();
    }

    // Allocate memory for current control's info
    snd_ctl_elem_info_alloca(&info);

    // Get control's infos
    if ((err = snd_hctl_elem_info(elem, info)) < 0) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "snd_hctl_elem_info error"))); // TODO refactor error
        return Null(isolate);
    }

    // All gonna be OK, instanciate a new v8 object
    obj = Object::New(isolate);

    // Set device name
    obj->Set(String::NewFromUtf8(isolate, "name"),
        String::NewFromUtf8(isolate, snd_ctl_elem_info_get_name(info)));

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
            obj->Set(String::NewFromUtf8(isolate, "items"),
                alsactl_fill_control_items(isolate, elem, info));
            break;
        case SND_CTL_ELEM_TYPE_BYTES:
            break;
        case SND_CTL_ELEM_TYPE_IEC958:
            break;
        case SND_CTL_ELEM_TYPE_INTEGER64:
            obj->Set(String::NewFromUtf8(isolate, "min"),
                Number::New(isolate, snd_ctl_elem_info_get_min64(info)));
            obj->Set(String::NewFromUtf8(isolate, "max"),
                Number::New(isolate, snd_ctl_elem_info_get_max64(info)));
            obj->Set(String::NewFromUtf8(isolate, "step"),
                Number::New(isolate, snd_ctl_elem_info_get_step64(info)));
            break;
        default:
            // TODO throw error
            break;
    }

    // Skip export of value if control is not readable
    if (snd_ctl_elem_info_is_readable(info)) {
        obj->Set(String::NewFromUtf8(isolate, "value"),
            alsactl_wrap_control_value(elem));
    }

    return obj;
}

void alsactl_list_elems(const v8::FunctionCallbackInfo<v8::Value>& args) {
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
        Local<Value> obj = alsactl_wrap_control(elem);
        result_list->Set(i++, obj);
    }

    // Close htcl handle
    snd_hctl_close(handle);

    args.GetReturnValue().Set(result_list);
}

void init(v8::Local<v8::Object> target) {
    NODE_SET_METHOD(target, "list_controls", alsactl_list_elems);
}

NODE_MODULE(alsactl, init);
