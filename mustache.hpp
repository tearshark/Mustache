//
//  Mustache
//  Created by Kevin Wojniak on 4/11/15.
//  Copyright (c) 2015 Kevin Wojniak. All rights reserved.
//

#ifndef MUSTACHE_HPP
#define MUSTACHE_HPP

#include <iostream>
#include <sstream>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>

namespace Mustache {

template <typename StringType>
StringType trim(const StringType &s) {
    auto it = s.begin();
    while (it != s.end() && isspace(static_cast<int>(*it))) {
        it++;
    }
    auto rit = s.rbegin();
    while (rit.base() != it && isspace(*rit)) {
        rit++;
    }
    return StringType(it, rit.base());
}

template <typename StringType>
StringType escape(const StringType& s) {
    StringType ret;
    ret.reserve(s.size()*2);
    for (const auto ch : s) {
        switch (static_cast<char>(ch)) {
            case '&':
                ret.append("&amp;");
                break;
            case '<':
                ret.append("&lt;");
                break;
            case '>':
                ret.append("&gt;");
                break;
            case '\"':
                ret.append("&quot;");
                break;
            case '\'':
                ret.append("&apos;");
                break;
            default:
                ret.append(1, ch);
                break;
        }
    }
    return ret;
}

template <typename StringType>
class Data {
public:
    enum class Type {
        Object,
        String,
        List,
        True,
        False,
    };
    
    using ObjectType = std::unordered_map<StringType, Data>;
    using ListType = std::vector<Data>;
    
    // Construction
    Data() : Data(Type::Object) {
    }
    Data(const StringType& string) : type_(Type::String) {
        str_.reset(new StringType(string));
    }
    Data(const typename StringType::value_type* string) : type_(Type::String) {
        str_.reset(new StringType(string));
    }
    Data(const ListType& list) : type_(Type::List) {
        list_.reset(new ListType(list));
    }
    Data(Type type) : type_(type) {
        switch (type_) {
            case Type::Object:
                obj_.reset(new ObjectType());
                break;
            case Type::String:
                str_.reset(new StringType());
                break;
            case Type::List:
                list_.reset(new ListType());
                break;
            default:
                break;
        }
    }
    Data(const StringType& name, const Data& var) : Data() {
        set(name, var);
    }
    static Data List() {
        return Data(Data::Type::List);
    }
    
    // Copying
    Data(const Data& data) : type_(data.type_) {
        if (data.obj_) {
            obj_.reset(new ObjectType(*data.obj_));
        } else if (data.str_) {
            str_.reset(new StringType(*data.str_));
        } else if (data.list_) {
            list_.reset(new ListType(*data.list_));
        }
    }
    Data& operator= (const Data& data) {
        if (&data != this) {
            type_ = data.type_;
            if (data.obj_) {
                obj_.reset(new ObjectType(*data.obj_));
            } else if (data.str_) {
                str_.reset(new StringType(*data.str_));
            } else if (data.list_) {
                list_.reset(new ListType(*data.list_));
            }
        }
        return *this;
    }
    
    // Type info
    Type type() const {
        return type_;
    }
    bool isObject() const {
        return type_ == Type::Object;
    }
    bool isString() const {
        return type_ == Type::String;
    }
    bool isList() const {
        return type_ == Type::List;
    }
    bool isBool() const {
        return type_ == Type::True || type_ == Type::False;
    }
    bool isTrue() const {
        return type_ == Type::True;
    }
    bool isFalse() const {
        return type_ == Type::False;
    }
    
    // Object data
    void set(const StringType& name, const Data& var) {
        if (isObject()) {
            obj_->insert(std::pair<StringType,Data>(name, var));
        }
    }
    bool exists(const StringType& name) {
        if (isObject()) {
            if (obj_->find(name) == obj_->end()) {
                return true;
            }
        }
        return false;
    }
    bool get(const StringType& name, Data& var) const {
        if (!isObject()) {
            return false;
        }
        const auto& it = obj_->find(name);
        if (it == obj_->end()) {
            return false;
        }
        var = it->second;
        return true;
    }
    
    // List data
    void push_back(const Data& var) {
        if (isList()) {
            list_->push_back(var);
        }
    }
    const Data& operator[] (size_t i) const {
        return list_->at(i);
    }
    const ListType& list() const {
        return *list_;
    }
    bool isEmptyList() const {
        return isList() && list_->empty();
    }
    bool isNonEmptyList() const {
        return isList() && !list_->empty();
    }
    
    // String data
    const StringType& stringValue() const {
        return *str_;
    }

private:
    Type type_;
    std::unique_ptr<ObjectType> obj_;
    std::unique_ptr<StringType> str_;
    std::unique_ptr<ListType> list_;
};

template <typename StringType>
class Context {
public:
    using DataType = Data<StringType>;
    
    Context(const DataType& data) {
        push(data);
    }
    
    void push(const DataType& data) {
        items_.insert(items_.begin(), &data);
    }
    
    void pop() {
        items_.erase(items_.begin());
    }
    
    bool get(const StringType& name, DataType& var) const {
        for (const auto& item : items_) {
            if (item->get(name, var)) {
                return true;
            }
        }
        return false;
    }
private:
    Context(const Context&);
    Context& operator= (const Context&);
    std::vector<const DataType*> items_;
};

// RAII wrapper for Context push/pop
template <typename StringType>
class ContextPusher {
public:
    ContextPusher(Context<StringType>& ctx, const Data<StringType> &data) : ctx_(ctx) {
        ctx.push(data);
    }
    ~ContextPusher() {
        ctx_.pop();
    }
private:
    ContextPusher(const ContextPusher&);
    ContextPusher& operator= (const ContextPusher&);
    Context<StringType>& ctx_;
};

template <typename StringType>
class Mustache {
public:
    Mustache(const StringType& input) {
        parse(input);
    }
    
    bool isValid() const {
        return errorMessage_.empty();
    }

    const StringType& errorMessage() const {
        return errorMessage_;
    }
    
    template <typename OStream>
    OStream& render(OStream& stream, const Data<StringType>& data) const {
        Context<StringType> ctx(data);
        walk([&stream, &ctx, this](Component& comp, int) -> WalkControl {
            return renderComponent(stream, ctx, comp);
        });
        return stream;
    }
    
    StringType render(const Data<StringType>& data) const {
        using streamstring = std::basic_ostringstream<typename StringType::value_type>;
        streamstring ss;
        return render(ss, data).str();
    }

    template <typename OStream>
    void print(OStream& stream) {
        walk([&stream](Component& comp, int depth) -> WalkControl {
            const StringType indent = depth >= 1 ? StringType(depth, ' ') : StringType();
            if (comp.isTag()) {
                stream << indent << "TAG: {{" << comp.tag.name << "}}" << std::endl;
            } else {
                stream << indent << "TXT: " << comp.text << std::endl;
            }
            return WalkControl::Continue;
        });
    }
    
private:
    using StringSizeType = typename StringType::size_type;

    class Tag {
    public:
        enum class Type {
            Invalid,
            Variable,
            UnescapedVariable,
            SectionBegin,
            SectionEnd,
            SectionBeginInverted,
            Comment,
            Partial,
            SetDelimiter,
        };
        StringType name;
        Type type = Type::Invalid;
        bool isSectionBegin() const {
            return type == Type::SectionBegin || type == Type::SectionBeginInverted;
        }
        bool isSectionEnd() const {
            return type == Type::SectionEnd;
        }
    };
    
    class Component {
    public:
        StringType text;
        Tag tag;
        std::vector<Component> children;
        StringSizeType position = StringType::npos;
        bool isText() const {
            return !text.empty();
        }
        bool isTag() const {
            return text.empty();
        }
    };
    
    void parse(const StringType& input) {
        using streamstring = std::basic_ostringstream<typename StringType::value_type>;
        
        const StringType braceDelimiterBegin(2, '{');
        const StringType braceDelimiterEnd(2, '}');
        const StringType braceDelimiterEndUnescaped(3, '}');
        const StringSizeType inputSize = input.size();
        
        StringType currentDelimiterBegin(braceDelimiterBegin);
        StringType currentDelimiterEnd(braceDelimiterEnd);
        bool currentDelimiterIsBrace = true;
        
        std::vector<Component*> sections;
        sections.push_back(&rootComponent_);
        
        StringSizeType inputPosition = 0;
        while (inputPosition != inputSize) {
            
            // Find the next tag start delimiter
            const StringSizeType tagLocationStart = input.find(currentDelimiterBegin, inputPosition);
            if (tagLocationStart == StringType::npos) {
                // No tag found. Add the remaining text.
                Component comp;
                comp.text = StringType(input, inputPosition, inputSize - inputPosition);
                comp.position = inputPosition;
                sections.back()->children.push_back(comp);
                break;
            } else if (tagLocationStart != inputPosition) {
                // Tag found, add text up to this tag.
                Component comp;
                comp.text = StringType(input, inputPosition, tagLocationStart - inputPosition);
                comp.position = inputPosition;
                sections.back()->children.push_back(comp);
            }
            
            // Find the next tag end delimiter
            StringSizeType tagContentsLocation = tagLocationStart + currentDelimiterBegin.size();
            const bool tagIsUnescapedVar = currentDelimiterIsBrace && tagLocationStart != (inputSize - 2) && input.at(tagContentsLocation) == braceDelimiterBegin.at(0);
            const StringType& currentTagDelimiterEnd(tagIsUnescapedVar ? braceDelimiterEndUnescaped : currentDelimiterEnd);
            const auto currentTagDelimiterEndSize = currentTagDelimiterEnd.size();
            if (tagIsUnescapedVar) {
                ++tagContentsLocation;
            }
            StringSizeType tagLocationEnd = input.find(currentTagDelimiterEnd, tagContentsLocation);
            if (tagLocationEnd == StringType::npos) {
                streamstring ss;
                ss << "No tag end delimiter found for start delimiter at " << tagLocationStart;
                errorMessage_.assign(ss.str());
                return;
            }
            
            // Parse tag
            StringType tagContents(trim(StringType(input, tagContentsLocation, tagLocationEnd - tagContentsLocation)));
            if (!tagContents.empty() && tagContents[0] == '=') {
                if (!parseSetDelimiterTag(tagContents, currentDelimiterBegin, currentDelimiterEnd)) {
                    streamstring ss;
                    ss << "Invalid set delimiter tag found at " << tagLocationStart;
                    errorMessage_.assign(ss.str());
                    return;
                }
                currentDelimiterIsBrace = currentDelimiterBegin == braceDelimiterBegin && currentDelimiterEnd == braceDelimiterEnd;
            }
            Component comp;
            parseTagContents(tagIsUnescapedVar, tagContents, comp.tag);
            comp.position = tagLocationStart;
            sections.back()->children.push_back(comp);
            
            // Push or pop sections
            if (comp.tag.isSectionBegin()) {
                sections.push_back(&sections.back()->children.back());
            } else if (comp.tag.isSectionEnd()) {
                if (sections.size() == 1) {
                    streamstring ss;
                    ss << "Section end tag \"" << comp.tag.name << "\" found without start tag at " << comp.position;
                    errorMessage_.assign(ss.str());
                    return;
                }
                sections.pop_back();
            }
            
            // Start next search after this tag
            inputPosition = tagLocationEnd + currentTagDelimiterEndSize;
        }
        
        // Check for sections without an ending tag
        const Component *invalidStartPosition = nullptr;
        walk([&invalidStartPosition](Component& comp, int) -> WalkControl {
            if (!comp.tag.isSectionBegin()) {
                return WalkControl::Continue;
            }
            if (comp.children.empty() || !comp.children.back().tag.isSectionEnd() || comp.children.back().tag.name != comp.tag.name) {
                invalidStartPosition = &comp;
                return WalkControl::Stop;
            }
            comp.children.pop_back(); // remove now useless end section component
            return WalkControl::Continue;
        });
        if (invalidStartPosition) {
            streamstring ss;
            ss << "No section end tag found for section \"" << invalidStartPosition->tag.name << "\" at " << invalidStartPosition->position;
            errorMessage_.assign(ss.str());
            return;
        }
    }
    
    enum class WalkControl {
        Continue,
        Stop,
        Skip,
    };
    using WalkCallback = std::function<WalkControl(Component&, int)>;
    
    void walk(const WalkCallback& callback) const {
        walkChildren(callback, rootComponent_);
    }

    void walkChildren(const WalkCallback& callback, const Component& comp) const {
        for (auto childComp : comp.children) {
            const WalkControl control = walkComponent(callback, childComp);
            if (control != WalkControl::Continue) {
                break;
            }
        }
    }
    
    WalkControl walkComponent(const WalkCallback& callback, Component& comp, int depth = 0) const {
        WalkControl control = callback(comp, depth);
        if (control == WalkControl::Stop) {
            return control;
        } else if (control == WalkControl::Skip) {
            return WalkControl::Continue;
        }
        ++depth;
        for (auto childComp : comp.children) {
            control = walkComponent(callback, childComp, depth);
            if (control == WalkControl::Stop) {
                return control;
            } else if (control == WalkControl::Skip) {
                control = WalkControl::Continue;
                break;
            }
        }
        --depth;
        return control;
    }
    
    bool parseSetDelimiterTag(const StringType& contents, StringType& startDelimiter, StringType& endDelimiter) {
        // Smallest legal tag is "=X X="
        if (contents.size() < 5) {
            return false;
        }
        // Must end with equal sign
        if (contents.back() != '=') {
            return false;
        }
        // Only 1 space character allowed
        const auto spacepos = contents.find(' ');
        if (spacepos == StringType::npos || contents.find(' ', spacepos + 1) != StringType::npos) {
            return false;
        }
        startDelimiter = contents.substr(1, spacepos - 1);
        endDelimiter = contents.substr(spacepos + 1, contents.size() - (spacepos + 2));
        return true;
    }
    
    void parseTagContents(bool isUnescapedVar, const StringType& contents, Tag& tag) {
        if (isUnescapedVar) {
            tag.type = Tag::Type::UnescapedVariable;
            tag.name = contents;
        } else if (contents.empty()) {
            tag.type = Tag::Type::Variable;
            tag.name.clear();
        } else {
            switch (static_cast<char>(contents.at(0))) {
                case '#':
                    tag.type = Tag::Type::SectionBegin;
                    break;
                case '^':
                    tag.type = Tag::Type::SectionBeginInverted;
                    break;
                case '/':
                    tag.type = Tag::Type::SectionEnd;
                    break;
                case '>':
                    tag.type = Tag::Type::Partial;
                    break;
                case '&':
                    tag.type = Tag::Type::UnescapedVariable;
                    break;
                case '!':
                    tag.type = Tag::Type::Comment;
                    break;
                default:
                    tag.type = Tag::Type::Variable;
                    break;
            }
            if (tag.type == Tag::Type::Variable) {
                tag.name = contents;
            } else {
                StringType name(contents);
                name.erase(name.begin());
                tag.name = trim(name);
            }
        }
    }
    
    template <typename OStream>
    WalkControl renderComponent(OStream& stream, Context<StringType>& ctx, Component& comp) const {
        if (comp.isText()) {
            stream << comp.text;
            return WalkControl::Continue;
        }
        
        const Tag& tag(comp.tag);
        Data<StringType> var;
        switch (tag.type) {
            case Tag::Type::Variable: {
                if (ctx.get(tag.name, var)) {
                    if (var.isString()) {
                        stream << escape(var.stringValue());
                    } else if (var.isBool()) {
                        stream << (var.isTrue() ? StringType("true") : StringType("false"));
                    }
                }
                break;
            }
            case Tag::Type::UnescapedVariable:
                if (ctx.get(tag.name, var)) {
                    if (var.isString()) {
                        stream << var.stringValue();
                    } else if (var.isBool()) {
                        stream << (var.isTrue() ? StringType("true") : StringType("false"));
                    }
                }
                break;
            case Tag::Type::SectionBegin:
                if (ctx.get(tag.name, var) && !var.isFalse() && !var.isEmptyList()) {
                    renderSection(stream, ctx, comp, var);
                }
                return WalkControl::Skip;
            case Tag::Type::SectionBeginInverted:
                if (!ctx.get(tag.name, var) || var.isFalse() || var.isEmptyList()) {
                    renderSection(stream, ctx, comp, var);
                }
                return WalkControl::Skip;
            case Tag::Type::Partial:
                std::cout << "RENDER PARTIAL: " << tag.name << std::endl;
                break;
            default:
                break;
        }
        
        return WalkControl::Continue;
    }
    
    template <typename OStream>
    void renderSection(OStream& stream, Context<StringType>& ctx, Component& incomp, const Data<StringType>& var) const {
        const auto callback = [&stream, &ctx, this](Component& comp, int) -> WalkControl {
            return renderComponent(stream, ctx, comp);
        };
        if (var.isNonEmptyList()) {
            for (const auto& item : var.list()) {
                ContextPusher<StringType> ctxpusher(ctx, item);
                walkChildren(callback, incomp);
            }
        } else if (var.isObject()) {
            ContextPusher<StringType> ctxpusher(ctx, var);
            walkChildren(callback, incomp);
        } else {
            walkChildren(callback, incomp);
        }
    }

private:
    StringType errorMessage_;
    Component rootComponent_;
};

} // namespace

#endif // MUSTACHE_HPP
