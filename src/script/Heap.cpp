#include "Heap.h"

#include <algorithm>

namespace easyforge::internal::scripting
{
    namespace
    {
        // The least a heap grows to before collecting, so small scripts never do.
        constexpr std::size_t SmallestCollection = 1024 * 1024;
    }

    const Value* TableObject::Find(TextObject* name) const
    {
        auto found = Positions.find(name);
        return found == Positions.end() ? nullptr : &Fields[found->second].second;
    }

    void TableObject::Set(TextObject* name, Value value)
    {
        auto found = Positions.find(name);
        if (found != Positions.end())
        {
            Fields[found->second].second = value;
            return;
        }
        Positions.emplace(name, Fields.size());
        Fields.emplace_back(name, value);
    }

    bool TableObject::Remove(TextObject* name)
    {
        auto found = Positions.find(name);
        if (found == Positions.end())
        {
            return false;
        }
        std::size_t position = found->second;
        Positions.erase(found);
        Fields.erase(Fields.begin() + static_cast<std::ptrdiff_t>(position));
        for (auto& [key, index] : Positions)
        {
            if (index > position)
            {
                --index;
            }
        }
        return true;
    }

    Heap::~Heap()
    {
        for (Object* object = First; object;)
        {
            Object* next = object->Next;
            delete object;
            object = next;
        }
    }

    void Heap::Track(Object* object)
    {
        object->Next = First;
        First = object;
        Used += object->Bytes;
    }

    void Heap::Grew(Object& object, std::size_t bytes)
    {
        object.Bytes += bytes;
        Used += bytes;
    }

    TextObject* Heap::Text(std::string_view text)
    {
        auto found = Interned.find(text);
        if (found != Interned.end())
        {
            return found->second;
        }
        TextObject* object = Make<TextObject>();
        object->Text = std::string(text);
        Grew(*object, object->Text.capacity());
        Interned.emplace(object->Text, object);
        return object;
    }

    void Heap::Mark(Value value)
    {
        if (value.IsReference())
        {
            Mark(value.Pointer);
        }
    }

    void Heap::Mark(Object* object)
    {
        if (!object || object->Marked)
        {
            return;
        }
        object->Marked = true;
        Gray.push_back(object);
    }

    namespace
    {
        void MarkPrototype(Heap& heap, const Prototype& prototype)
        {
            for (const Value& constant : prototype.Constants)
            {
                heap.Mark(constant);
            }
            for (const std::unique_ptr<Prototype>& child : prototype.Children)
            {
                MarkPrototype(heap, *child);
            }
        }
    }

    void Heap::Trace()
    {
        while (!Gray.empty())
        {
            Object* object = Gray.back();
            Gray.pop_back();
            switch (object->Kind)
            {
            case ObjectKind::Text:
            case ObjectKind::Host: break;
            case ObjectKind::List:
                for (const Value& item : static_cast<ListObject*>(object)->Items)
                {
                    Mark(item);
                }
                break;
            case ObjectKind::Table:
            {
                auto* table = static_cast<TableObject*>(object);
                for (const auto& [name, value] : table->Fields)
                {
                    Mark(name);
                    Mark(value);
                }
                Mark(table->Record);
                break;
            }
            case ObjectKind::Function:
            {
                auto* function = static_cast<FunctionObject*>(object);
                Mark(function->Module);
                for (BoxObject* box : function->Captures)
                {
                    Mark(box);
                }
                break;
            }
            case ObjectKind::Native: Mark(static_cast<NativeObject*>(object)->Bound); break;
            case ObjectKind::Record:
            {
                auto* record = static_cast<RecordObject*>(object);
                Mark(record->Name);
                for (TextObject* field : record->Fields)
                {
                    Mark(field);
                }
                break;
            }
            case ObjectKind::Box: Mark(static_cast<BoxObject*>(object)->Held); break;
            case ObjectKind::Module:
            {
                auto* module = static_cast<ModuleObject*>(object);
                Mark(module->Globals);
                for (const std::unique_ptr<Prototype>& code : module->Code)
                {
                    MarkPrototype(*this, *code);
                }
                break;
            }
            }
        }
    }

    void Heap::Sweep()
    {
        Trace();
        Object** link = &First;
        while (Object* object = *link)
        {
            if (object->Marked)
            {
                object->Marked = false;
                link = &object->Next;
                continue;
            }
            *link = object->Next;
            if (object->Kind == ObjectKind::Text)
            {
                Interned.erase(static_cast<TextObject*>(object)->Text);
            }
            Used -= std::min(Used, object->Bytes);
            delete object;
        }
        NextCollection = std::max(Used * 2, SmallestCollection);
    }
}
