//
// yaclap.hpp
// Yet Another Command Line Argument Parser
// https://github.com/sgrottel/yaclap
//
// MIT License
//
// Copyright(c) 2024-2026 Sebastian Grottel
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
#pragma once

// yaclap semantic version: MAJOR.MINOR.PATCH(.BUILD)
#define YACLAP_VERSION_MAJOR 0
#define YACLAP_VERSION_MINOR 4
#define YACLAP_VERSION_PATCH 2
#define YACLAP_VERSION_BUILD 0
#define YACLAP_VERSION_GITHASHSTR ""

#if !defined(__cplusplus)
#error yaclap.hpp must be compiled as cplusplus source code
#endif
#if (__cplusplus < 201700L) && (!defined(_MSVC_LANG) || (_MSVC_LANG < 201700L))
#error yaclap.hpp requires cpp std 2017 or newer
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace yaclap
{

    template <typename CHAR>
    class Alias
    {
    public:
        enum class StringCompare
        {
            CaseSensitive,
            CaseInsensitive
        };

        Alias() noexcept = default;

        Alias(const CHAR* name, StringCompare stringCompare = StringCompare::CaseSensitive)
            : m_name{ name }, m_stringCompare{ stringCompare }
        {
        }

        template <typename T, typename A>
        Alias(const std::basic_string<CHAR, T, A>& name, StringCompare stringCompare = StringCompare::CaseSensitive)
            : m_name{ name }, m_stringCompare{ stringCompare }
        {
        }

        template <typename T>
        Alias(const std::basic_string_view<CHAR, T>& name, StringCompare stringCompare = StringCompare::CaseSensitive)
            : m_name{ name }, m_stringCompare{ stringCompare }
        {
        }

        const std::basic_string<CHAR>& GetName() const noexcept
        {
            return m_name;
        }

        StringCompare GetStringCompareMode() const noexcept
        {
            return m_stringCompare;
        }

        template <typename T>
        bool IsMatch(const std::basic_string_view<CHAR, T>& s) const
        {
            if (s.size() != m_name.size())
            {
                return false;
            }

            if (m_stringCompare == StringCompare::CaseInsensitive)
            {
                for (size_t i = 0; i < s.size(); ++i)
                {
                    if (!AreCharEqualCaseInsenstive(m_name[i], s[i]))
                    {
                        return false;
                    }
                }
                return true;
            }

            for (size_t i = 0; i < s.size(); ++i)
            {
                if (m_name[i] != s[i])
                {
                    return false;
                }
            }
            return true;
        }

    private:
        std::basic_string<CHAR> m_name;
        StringCompare m_stringCompare;

        static inline bool AreCharEqualCaseInsenstive(CHAR const& a, CHAR const& b);
    };

    template <>
    inline bool Alias<char>::AreCharEqualCaseInsenstive(char const& a, char const& b)
    {
        return std::tolower(a) == std::tolower(b);
    }

    template <>
    inline bool Alias<wchar_t>::AreCharEqualCaseInsenstive(wchar_t const& a, wchar_t const& b)
    {
        return std::towlower(a) == std::towlower(b);
    }

    template <typename CHAR>
    class WithNameAndAlias
    {
    protected:
        WithNameAndAlias() noexcept = default;

        WithNameAndAlias(const Alias<CHAR>& name)
        {
            AddAliasImpl(name);
        }

        void AddAliasImpl(const Alias<CHAR>& alias)
        {
            if (alias.GetName().empty())
            {
                throw std::invalid_argument("alias");
            }
            m_names.push_back(alias);
        }

        void SetHideFromHelpImpl() noexcept
        {
            m_hideFromHelp = true;
        }

    public:
        inline typename std::vector<Alias<CHAR>>::const_iterator NameAliasBegin() const
        {
            return m_names.cbegin();
        }

        inline typename std::vector<Alias<CHAR>>::const_iterator NameAliasEnd() const
        {
            return m_names.cend();
        }

        std::basic_string<CHAR> GetAllNames() const;

        template <typename T>
        bool IsMatch(const std::basic_string_view<CHAR, T>& s) const
        {
            for (Alias<CHAR> const& a : m_names)
            {
                if (a.IsMatch(s))
                {
                    return true;
                }
            }
            return false;
        }

        inline bool DoesHideFromHelp() const noexcept
        {
            return m_hideFromHelp;
        }

    private:
        std::vector<Alias<CHAR>> m_names;
        bool m_hideFromHelp{ false };
    };

    template <typename CHAR>
    class WithName
    {
    protected:
        WithName() noexcept = default;

        template <typename TSTR>
        WithName(const TSTR& name)
            : m_name{ name }
        {
        }

    public:
        inline const std::basic_string<CHAR>& GetName() const noexcept
        {
            return m_name;
        }

    private:
        std::basic_string<CHAR> m_name;
    };

    template <typename CHAR>
    class WithDescription
    {
    protected:
        WithDescription() noexcept = default;

        template <typename TSTR>
        WithDescription(const TSTR& description)
            : m_desc{ description }
        {
        }

    public:
        inline const std::basic_string<CHAR>& GetDescription() const noexcept
        {
            return m_desc;
        }

    private:
        std::basic_string<CHAR> m_desc;
    };

    template <typename CHAR>
    class WithIdentity
    {
    private:
        static uint32_t c_nextId;
        uint32_t m_id = 0;

    public:
        WithIdentity() noexcept
            : m_id{ c_nextId++ }
        {
        }

        WithIdentity(const WithIdentity& src) noexcept
            : m_id{ src.m_id }
        {
        }

        WithIdentity(WithIdentity&& src) noexcept
            : m_id{ src.m_id }
        {
            src.m_id = c_nextId++;
        }

        WithIdentity& operator=(const WithIdentity& src) noexcept
        {
            m_id = src.m_id;
            return *this;
        }

        WithIdentity& operator=(WithIdentity&& src) noexcept
        {
            m_id = src.m_id;
            src.m_id = c_nextId++;
            return *this;
        }

        inline auto GetId() const noexcept
        {
            return m_id;
        }

        template <typename A, typename B>
        static inline bool Equals(const A& a, const B& b) noexcept
        {
            return static_cast<WithIdentity<CHAR> const&>(a).m_id == static_cast<WithIdentity<CHAR> const&>(b).m_id;
        }
    };

    template <typename CHAR>
    uint32_t WithIdentity<CHAR>::c_nextId = 1;

    template <typename CHAR>
    class Argument : public WithIdentity<CHAR>, public WithName<CHAR>, public WithDescription<CHAR>
    {
    public:
        static constexpr bool NotRequired = false;

        Argument() noexcept = default;

        template <typename TSTR1, typename TSTR2>
        explicit Argument(const TSTR1& name, const TSTR2& description, bool isRequired = true)
            : WithName<CHAR>{ name }, WithDescription<CHAR>{description}, m_isRequired{ isRequired }
        {
        }

        inline bool IsRequired() const noexcept
        {
            return m_isRequired;
        }

    private:
        bool m_isRequired;
    };

    template <typename CHAR>
    class Option : public WithIdentity<CHAR>, public WithNameAndAlias<CHAR>, public WithDescription<CHAR>
    {
    public:
        Option() noexcept = default;

        template <typename TSTR1, typename TSTR2>
        explicit Option(const Alias<CHAR>& name, const TSTR1& argumentName, const TSTR2& description)
            : WithNameAndAlias<CHAR>{ name }, WithDescription<CHAR>{description}, m_argName{ argumentName }
        {
        }

        Option& AddAlias(const Alias<CHAR>& alias)
        {
            WithNameAndAlias<CHAR>::AddAliasImpl(alias);
            return *this;
        }

        Option& HideFromHelp() noexcept
        {
            WithNameAndAlias<CHAR>::SetHideFromHelpImpl();
            return *this;
        }

        inline const std::basic_string<CHAR>& GetArgumentName() const noexcept
        {
            return m_argName;
        }

        template <typename T1, typename T2>
        bool IsMatchWithValue(const std::basic_string_view<CHAR, T1>& s,
            std::basic_string_view<CHAR, T2>& outValueStr) const
        {
            for (typename std::vector<Alias<CHAR>>::const_iterator a = WithNameAndAlias<CHAR>::NameAliasBegin();
                a != WithNameAndAlias<CHAR>::NameAliasEnd(); ++a)
            {
                size_t nameLen = a->GetName().size();
                if (nameLen >= s.size())
                {
                    return false;
                }
                // If option name and value were quoted together, being one arg string, e.g. ` \"-do something\" `,
                // then the space char is interpreted as separator, making the input the same as `"-do" "something"`.
                if (s[nameLen] != ':' && s[nameLen] != ' ' && s[nameLen] != '=')
                {
                    return false;
                }
                std::basic_string_view<CHAR> sub{ s.data(), nameLen };

                if (a->IsMatch(sub))
                {
                    outValueStr = std::basic_string_view<CHAR>{ s.data() + nameLen + 1, s.size() - nameLen - 1 };
                    return true;
                }
            }

            return false;
        }

    private:
        std::basic_string<CHAR> m_argName;
    };

    template <typename CHAR>
    class Switch : public WithIdentity<CHAR>, public WithNameAndAlias<CHAR>, public WithDescription<CHAR>
    {
    public:
        Switch() noexcept = default;

        template <typename TSTR>
        explicit Switch(const Alias<CHAR>& name, const TSTR& description)
            : WithNameAndAlias<CHAR>{ name }, WithDescription<CHAR>{description}
        {
        }

        Switch& AddAlias(const Alias<CHAR>& alias)
        {
            WithNameAndAlias<CHAR>::AddAliasImpl(alias);
            return *this;
        }

        Switch& HideFromHelp() noexcept
        {
            WithNameAndAlias<CHAR>::SetHideFromHelpImpl();
            return *this;
        }
    };

    template <typename CHAR>
    class Command;

    template <typename CHAR>
    class WithCommandContainer
    {
    protected:
        WithCommandContainer() noexcept = default;

        void AddCommandImpl(const Command<CHAR>& command);

        void AddOptionImpl(const Option<CHAR>& option)
        {
            if (option.NameAliasBegin() == option.NameAliasEnd())
            {
                throw std::invalid_argument("option");
            }
            m_options.push_back(option);
        }

        void AddSwitchImpl(const Switch<CHAR>& switchOption)
        {
            if (switchOption.NameAliasBegin() == switchOption.NameAliasEnd())
            {
                throw std::invalid_argument("switchOption");
            }
            m_switches.push_back(switchOption);
        }

        void AddArgumentImpl(const Argument<CHAR>& argument)
        {
            m_arguments.push_back(argument);
        }

    public:
        inline typename std::vector<Command<CHAR>>::const_iterator CommandsBegin() const
        {
            return m_commands.cbegin();
        }

        inline typename std::vector<Command<CHAR>>::const_iterator CommandsEnd() const
        {
            return m_commands.cend();
        }

        inline typename std::vector<Option<CHAR>>::const_iterator OptionsBegin() const
        {
            return m_options.cbegin();
        }

        inline typename std::vector<Option<CHAR>>::const_iterator OptionsEnd() const
        {
            return m_options.cend();
        }

        inline typename std::vector<Switch<CHAR>>::const_iterator SwitchesBegin() const
        {
            return m_switches.cbegin();
        }

        inline typename std::vector<Switch<CHAR>>::const_iterator SwitchesEnd() const
        {
            return m_switches.cend();
        }

        inline typename std::vector<Argument<CHAR>>::const_iterator ArgumentsBegin() const
        {
            return m_arguments.cbegin();
        }

        inline typename std::vector<Argument<CHAR>>::const_iterator ArgumentsEnd() const
        {
            return m_arguments.cend();
        }

    private:
        std::vector<Command<CHAR>> m_commands;
        std::vector<Option<CHAR>> m_options;
        std::vector<Switch<CHAR>> m_switches;
        std::vector<Argument<CHAR>> m_arguments;
    };

    template <typename CHAR>
    class Command : public WithIdentity<CHAR>,
        public WithCommandContainer<CHAR>,
        public WithNameAndAlias<CHAR>,
        public WithDescription<CHAR>
    {
    public:
        Command() noexcept = default;

        template <typename TSTR>
        Command(const Alias<CHAR>& name, const TSTR& description)
            : WithCommandContainer<CHAR>{}, WithNameAndAlias<CHAR>{name}, WithDescription<CHAR>{description}
        {
        }

        Command& AddAlias(const Alias<CHAR>& alias)
        {
            WithNameAndAlias<CHAR>::AddAliasImpl(alias);
            return *this;
        }

        Command& HideFromHelp() noexcept
        {
            WithNameAndAlias<CHAR>::SetHideFromHelpImpl();
            return *this;
        }

        Command& AddCommand(const Command<CHAR>& command)
        {
            WithCommandContainer<CHAR>::AddCommandImpl(command);
            return *this;
        }

        Command& AddOption(const Option<CHAR>& option)
        {
            WithCommandContainer<CHAR>::AddOptionImpl(option);
            return *this;
        }

        Command& AddSwitch(const Switch<CHAR>& switchOption)
        {
            WithCommandContainer<CHAR>::AddSwitchImpl(switchOption);
            return *this;
        }

        Command& AddArgument(const Argument<CHAR>& argument)
        {
            WithCommandContainer<CHAR>::AddArgumentImpl(argument);
            return *this;
        }

        Command& Add(const Command<CHAR>& command)
        {
            return AddCommand(command);
        }

        Command& Add(const Option<CHAR>& option)
        {
            return AddOption(option);
        }

        Command& Add(const Switch<CHAR>& switchOption)
        {
            return AddSwitch(switchOption);
        }

        Command& Add(const Argument<CHAR>& argument)
        {
            return AddArgument(argument);
        }

        enum class OnUnmatchedArguments
        {
            Keep, //< value set by parser or parent command will not be kept during the parsing process
            SetError,
            NoError
        };

        inline void SetErrorOnUnmatchedArguments(OnUnmatchedArguments setError = OnUnmatchedArguments::SetError) noexcept
        {
            m_errorOnUnmatchedArguments = setError;
        }

        inline OnUnmatchedArguments GetSetErrorOnUnmatchedArguments() const noexcept
        {
            return m_errorOnUnmatchedArguments;
        }

    private:
        OnUnmatchedArguments m_errorOnUnmatchedArguments{ OnUnmatchedArguments::Keep };
    };

    template <typename CHAR>
    void WithCommandContainer<CHAR>::AddCommandImpl(const Command<CHAR>& command)
    {
        if (command.NameAliasBegin() == command.NameAliasEnd())
        {
            throw std::invalid_argument("command");
        }
        m_commands.push_back(command);
    }

    template <typename CHAR>
    class Parser : public WithCommandContainer<CHAR>, public WithName<CHAR>, public WithDescription<CHAR>
    {
    public:
        template <typename TSTR1, typename TSTR2>
        Parser(const TSTR1& name, const TSTR2& description)
            : WithCommandContainer<CHAR>{}, WithName<CHAR>{name}, WithDescription<CHAR>{description}
        {
        }

        Parser& AddCommand(const Command<CHAR>& command)
        {
            WithCommandContainer<CHAR>::AddCommandImpl(command);
            return *this;
        }

        Parser& AddOption(const Option<CHAR>& option)
        {
            WithCommandContainer<CHAR>::AddOptionImpl(option);
            return *this;
        }

        Parser& AddSwitch(const Switch<CHAR>& switchOption)
        {
            WithCommandContainer<CHAR>::AddSwitchImpl(switchOption);
            return *this;
        }

        Parser& AddArgument(const Argument<CHAR>& argument)
        {
            WithCommandContainer<CHAR>::AddArgumentImpl(argument);
            return *this;
        }

        Parser& Add(const Command<CHAR>& command)
        {
            return AddCommand(command);
        }

        Parser& Add(const Option<CHAR>& option)
        {
            return AddOption(option);
        }

        Parser& Add(const Switch<CHAR>& switchOption)
        {
            return AddSwitch(switchOption);
        }

        Parser& Add(const Argument<CHAR>& argument)
        {
            return AddArgument(argument);
        }

        inline void EnableImplicitHelpSwitch(bool enable = true) noexcept
        {
            m_withImplicitHelpSwitch = enable;
        }

        inline bool IsImplicitHelpSwitchEnabled() const noexcept
        {
            return m_withImplicitHelpSwitch;
        }

        inline void SetErrorOnUnmatchedArguments(bool setError = true) noexcept
        {
            m_errorOnUnmatchedArguments = setError;
        }

        inline bool IsSetErrorOnUnmatchedArguments() const noexcept
        {
            return m_errorOnUnmatchedArguments;
        }

        class ResultErrorInfo
        {
        public:
            /// <summary>
            /// Returns true if this is the Result of a successful parsing,
            /// i.e. no errors were encountered, and the implicit help switch was not triggered.
            /// </summary>
            inline bool IsSuccess() const noexcept
            {
                return m_success;
            }

            /// <summary>
            /// Returns true if the help information should be shown after assigning all results
            /// </summary>
            inline bool ShouldShowHelp() const noexcept
            {
                return m_shouldShowHelp;
            }

            /// <summary>
            /// Sets the error message to be shown to the user
            /// </summary>
            inline void SetError(const CHAR* message, bool setUnsuccessful = true)
            {
                if (m_error.empty() && (message != nullptr || *message != static_cast<CHAR>(0)))
                {
                    m_error = message;
                }
                if (setUnsuccessful)
                {
                    m_success = false;
                    m_shouldShowHelp = true;
                }
            }

            /// <summary>
            /// Sets the error message to be shown to the user
            /// </summary>
            template <typename T, typename A>
            inline void SetError(const std::basic_string<CHAR, T, A>& message, bool setUnsuccessful = true)
            {
                SetError(message.c_str(), setUnsuccessful);
            }

            /// <summary>
            /// Gets the set error message
            /// </summary>
            inline std::basic_string<CHAR> const& GetError() const noexcept
            {
                return m_error;
            }

            inline void SetShouldShowHelp()
            {
                m_shouldShowHelp = true;
            }

            inline void SetSuccess()
            {
                m_success = true;
                m_error.clear();
            }

        private:
            bool m_success = false;
            bool m_shouldShowHelp = false;
            std::basic_string<CHAR> m_error{};
        };

        class ResultValueView : public std::basic_string_view<CHAR>
        {
        public:
            ResultValueView()
                : m_errorInfo{ std::make_shared<ResultErrorInfo>() }, m_source{ std::nullopt }, m_position{ -1 }
            {
            }

            ResultValueView(ResultValueView const& src) = default;

            ResultValueView(ResultValueView&& src) = default;

            inline std::optional<WithIdentity<CHAR>> const& GetSource() const noexcept
            {
                return m_source;
            }

            inline bool IsFromSource(WithIdentity<CHAR> const& src) const
            {
                if (!m_source.has_value())
                {
                    return false;
                }
                return WithIdentity<CHAR>::Equals(m_source.value(), src);
            }

            inline bool HasValue() const noexcept
            {
                return m_position >= 0;
            }

            inline operator bool() const noexcept
            {
                return HasValue();
            }

            inline int GetPosition() const noexcept
            {
                return m_position;
            }

            /// <summary>
            /// Converts a string optional return from this result into an integer (signed 64bit long long) optional.
            /// If the string cannot be parsed and converted, an error will be set on this result object, unless
            /// deactivated with the optional bool parameter, and the function will return a null optional.
            /// </summary>
            /// <remarks>
            /// Syntax:
            ///     [+-]?[0..9]+
            ///   Optional sign, followed by number characters in base-10
            ///     [+-]?[oO][0..7]+
            ///   Optional sign, 'o' marker, followed by octal-number characters in base-8
            ///     [+-]?[xX][0..9a..fA..F]+
            ///   Optional sign, 'x' marker, followed by hex-number characters in base-16
            ///     [+-]?[bB][0..1]+
            ///   Optional sign, 'b' marker, followed by binary-number characters in base-2
            /// </remarks>
            std::optional<long long> AsInteger(bool errorWhenTypeParsingFails = true) const;

            /// <summary>
            /// Converts a string optional return from this result into a floating-point (64bit double) optional.
            /// If the string cannot be parsed and converted, an error will be set on this result object, unless
            /// deactivated with the optional bool parameter, and the function will return a null optional.
            /// </summary>
            /// <remarks>
            /// Syntax:
            ///     [+-]?[0..9]*\.?[0..9]*([eE][+-]?[0..9]+)?
            ///   The three digit sequences are: whole number, fraction, exponent.
            ///   Either whole number or fraction must be present. Whole number can include a trailing dot. Fraction
            ///   must start with the dot. Sign character is optional. Exponent must start with the e-marker. Exponent
            ///   is optional. Exponent sign is optional.
            /// </remarks>
            std::optional<double> AsDouble(bool errorWhenTypeParsingFails = true) const;

            /// <summary>
            /// Converts a string optional return from this result into a boolean (bool) optional.
            /// If the string cannot be parsed and converted, an error will be set on this result object, unless
            /// deactivated with the optional bool parameter, and the function will return a null optional.
            /// </summary>
            /// <remarks>
            /// Syntax:
            ///     true, t, on, yes, y
            ///   If the input string, trimmed from leading and trailing whitespace, matches on of these words
            ///   case-insenstively, the return value is "true"
            ///     false, f, off, no, n
            ///   If the input string, trimmed from leading and trailing whitespace, matches on of these words
            ///   case-insenstively, the return value is "false"
            ///     (int)
            ///   Else, if the input string can be parsed as integer (cf. AsInteger), the return value is "(int) != 0"
            /// </remarks>
            std::optional<bool> AsBool(bool errorWhenTypeParsingFails = true) const;

        protected:
            ResultValueView(std::basic_string_view<CHAR> str, std::shared_ptr<ResultErrorInfo> errorInfo,
                std::optional<WithIdentity<CHAR>> source, int position)
                : std::basic_string_view<CHAR>{ str }, m_errorInfo{ errorInfo }, m_source{ source }, m_position{ position }
            {
            }

        private:
            inline auto GetStringTrimmed() const
            {
                auto b = std::basic_string_view<CHAR>::cbegin();
                auto e = std::basic_string_view<CHAR>::cend();
                while (b != e && StringConsts::isspace(*b))
                {
                    b++;
                }
                while (b != e)
                {
                    auto i = e - 1;
                    if (i == b || !StringConsts::isspace(*i))
                    {
                        break;
                    }
                    e = i;
                }
                return std::make_pair(b, e);
            }

            std::shared_ptr<ResultErrorInfo> m_errorInfo;
            std::optional<WithIdentity<CHAR>> m_source;
            int m_position;
        };

        /// <summary>
        /// The parse result only identifies commands, options, switches, and arguments.
        /// Use additional calls on this object to convert and assign values.
        /// </summary>
        class Result
        {
        public:
            using string_t = std::basic_string<CHAR>;
            using string_view_t = std::basic_string_view<CHAR>;

            /// <summary>
            /// Returns true if this is the Result of a successful parsing,
            /// i.e. no errors were encountered, and the implicit help switch was not triggered.
            /// </summary>
            inline bool IsSuccess() const noexcept
            {
                return m_errorInfo->IsSuccess();
            }

            /// <summary>
            /// Returns true if the help information should be shown after assigning all results
            /// </summary>
            inline bool ShouldShowHelp() const noexcept
            {
                return m_errorInfo->ShouldShowHelp();
            }

            /// <summary>
            /// Sets the error message to be shown to the user
            /// </summary>
            inline void SetError(const CHAR* message, bool setUnsuccessful = true)
            {
                m_errorInfo->SetError(message, setUnsuccessful);
            }

            /// <summary>
            /// Sets the error message to be shown to the user
            /// </summary>
            inline void SetError(const string_t& message, bool setUnsuccessful = true)
            {
                m_errorInfo->SetError(message.c_str(), setUnsuccessful);
            }

            /// <summary>
            /// Gets the set error message
            /// </summary>
            inline string_t const& GetError() const noexcept
            {
                return m_errorInfo->GetError();
            }

            /// <summary>
            /// Prints to set error message to std out
            /// </summary>
            inline void PrintError(bool tryUseColor = true) const;

            /// <summary>
            /// Prints the error message to the specified stream
            /// </summary>
            template <typename TSTREAMT = typename std::basic_ostream<CHAR>::traits_type>
            void PrintError(std::basic_ostream<CHAR, TSTREAMT>& stream, bool tryUseColor = true) const;

            /// <summary>
            /// Returns all Commands occured in the command line in order in which they appeared.
            /// </summary>
            inline std::vector<WithIdentity<CHAR>> const& Commands() const noexcept
            {
                return m_commands;
            }

            /// <summary>
            /// Returns true if the specified Command `cmd` occured in the command line.
            /// </summary>
            inline bool HasCommand(Command<CHAR> const& cmd) const noexcept
            {
                for (auto const& cmdId : m_commands)
                {
                    if (WithIdentity<CHAR>::Equals(cmdId, cmd))
                    {
                        return true;
                    }
                }
                return false;
            }

            /// <summary>
            /// Returns all Options occured in the command line in order in which they appeared.
            /// </summary>
            inline std::vector<ResultValueView> const& Options() const noexcept
            {
                return m_options;
            }

            /// <summary>
            /// Returns the number of times the specified option `opt` was seen in the command line (0 = never).
            /// </summary>
            inline size_t GetOptionCount(Option<CHAR> const& opt) const
            {
                return std::count_if(m_options.cbegin(), m_options.cend(),
                    [&opt](ResultValueView const& o) { return o.IsFromSource(opt); });
            }

            /// <summary>
            /// Returns the value of the _first_ occurance of the specified Option `opt` in the command line.
            /// </summary>
            inline ResultValueView GetOptionValue(Option<CHAR> const& opt) const
            {
                for (ResultValueView const& o : m_options)
                {
                    if (o.IsFromSource(opt))
                    {
                        return o;
                    }
                }
                return {};
            }

            /// <summary>
            /// Alias for 'true', useful as human-readable value for the second parameter of `GetOptionValue`
            /// </summary>
            static constexpr bool ErrorIfMultiple = true;

            /// <summary>
            /// Returns the value of the _first_ occurance of the specified Option `opt` in the command line.
            /// If `setErrorIfMultiple` is set to `ErrorIfMultiple` or `true`, and the Option was found more than one
            /// time in the command line, then an error message is set in the result object, and `nullopt` is returned.
            /// </summary>
            inline ResultValueView GetOptionValue(Option<CHAR> const& opt, bool setErrorIfMultiple)
            {
                if (GetOptionCount(opt) > 1)
                {
                    if (setErrorIfMultiple)
                    {
                        m_errorInfo->SetError(string_t{ StringConsts::errorOptionSpecifiedMultipletimes } +
                            opt.NameAliasBegin()->GetName());
                    }
                    return {};
                }
                return GetOptionValue(opt);
            }

            /// <summary>
            /// Returns all values of all occurances of the specified Option `opt` in the command line.
            /// </summary>
            inline std::vector<ResultValueView> GetOptionValues(Option<CHAR> const& opt) const
            {
                std::vector<ResultValueView> result;
                for (ResultValueView const& o : m_options)
                {
                    if (o.IsFromSource(opt))
                    {
                        result.push_back(o);
                    }
                }
                return result;
            }

            /// <summary>
            /// Returns all Switches occured in the command line in the order in which they appeared
            /// </summary>
            inline std::vector<WithIdentity<CHAR>> const& Switches() const noexcept
            {
                return m_switches;
            }

            /// <summary>
            /// Returns the number of times the specified switch `swt` was seen in the command line (0 = never).
            /// </summary>
            inline size_t HasSwitch(Switch<CHAR> const& swt) const
            {
                return std::count_if(m_switches.cbegin(), m_switches.cend(), [&swt](WithIdentity<CHAR> const& s)
                    { return WithIdentity<CHAR>::Equals(s, swt); });
            }

            /// <summary>
            /// Returns all matched Arguments occured in the command line in the order in which they appeared
            /// </summary>
            inline std::vector<ResultValueView> const& MatchedArguments() const noexcept
            {
                return m_matchedArguments;
            }

            /// <summary>
            /// Gets the value of the specified Argument `arg`
            /// </summary>
            inline ResultValueView GetArgument(Argument<CHAR> const& arg) const
            {
                for (auto const& matched : m_matchedArguments)
                {
                    if (matched.IsFromSource(arg))
                    {
                        return matched;
                    }
                }
                return {};
            }

            /// <summary>
            /// Returns all unmatched arguments from the command line.
            /// </summary>
            inline std::vector<ResultValueView> const& UnmatchedArguments() const noexcept
            {
                return m_unmatchedArguments;
            }

            /// <summary>
            /// Returns true if there is at least one unmatched argument.
            /// </summary>
            inline bool HasUnmatchedArguments() const noexcept
            {
                return !m_unmatchedArguments.empty();
            }

        protected:
            Result() = default;

            inline void AddCommand(Command<CHAR> const& cmd)
            {
                m_commands.push_back(cmd);
            }

            inline void AddOption(ResultValueView&& rv)
            {
                m_options.push_back(std::move(rv));
            }

            inline void AddSwitch(Switch<CHAR> const& swt)
            {
                m_switches.push_back(swt);
            }

            inline void AddMatchedArgument(ResultValueView&& rv)
            {
                m_matchedArguments.push_back(std::move(rv));
            }

            inline void AddUnmatchedArgument(ResultValueView&& rv)
            {
                m_unmatchedArguments.push_back(std::move(rv));
            }

            inline std::shared_ptr<ResultErrorInfo> GetErrorInfo()
            {
                return m_errorInfo;
            }

        private:
            std::shared_ptr<ResultErrorInfo> m_errorInfo{ std::make_shared<ResultErrorInfo>() };

            std::vector<WithIdentity<CHAR>> m_commands;
            std::vector<ResultValueView> m_options;
            std::vector<WithIdentity<CHAR>> m_switches;
            std::vector<ResultValueView> m_matchedArguments;
            std::vector<ResultValueView> m_unmatchedArguments;
        };

        /// <summary>
        /// Parses the specified command line and returns the parse result
        /// </summary>
        Result Parse(int argc, const CHAR* const* argv, bool skipFirstArg = true) const;

        /// <summary>
        /// Prints a user-readable help text
        /// </summary>
        template <typename TSTREAMT = typename std::basic_ostream<CHAR>::traits_type>
        inline void PrintHelp(Command<CHAR> const& command, std::basic_ostream<CHAR, TSTREAMT>& stream) const;

        /// <summary>
        /// Prints a user-readable help text
        /// </summary>
        template <typename TSTREAMT = typename std::basic_ostream<CHAR>::traits_type>
        inline void PrintHelp(Result const& result, std::basic_ostream<CHAR, TSTREAMT>& stream) const;

        /// <summary>
        /// Prints a user-readable help text
        /// </summary>
        template <typename TSTREAMT = typename std::basic_ostream<CHAR>::traits_type>
        inline void PrintHelp(std::basic_ostream<CHAR, TSTREAMT>& stream) const;

        /// <summary>
        /// Prints a user-readable help text
        /// </summary>
        inline void PrintHelp(Command<CHAR> const& command) const;

        /// <summary>
        /// Prints a user-readable help text
        /// </summary>
        inline void PrintHelp(Result const& result) const;

        /// <summary>
        /// Prints a user-readable help text
        /// </summary>
        inline void PrintHelp() const;

        /// <summary>
        /// If `result.IsSuccess() == false` prints the error message.
        /// If `result.ShouldShowHelp() == true` prints the usage information.
        /// Both info is printed to `stdout`.
        /// </summary>
        inline void PrintErrorAndHelpIfNeeded(Result const& result) const;

        /// <summary>
        /// If `result.IsSuccess() == false` prints the error message.
        /// If `result.ShouldShowHelp() == true` prints the usage information.
        /// Both info is printed to the specified `stream`.
        /// </summary>
        template <typename TSTREAMT = typename std::basic_ostream<CHAR>::traits_type>
        inline void PrintErrorAndHelpIfNeeded(Result const& result, std::basic_ostream<CHAR, TSTREAMT>& stream) const;

    private:
        struct StringConsts;

        inline static constexpr size_t cexprStrLen(const CHAR* str) noexcept
        {
            size_t length = 0;
            while (*str++)
            {
                ++length;
            }
            return length;
        }

        template <typename TSTREAMT = typename std::basic_ostream<CHAR>::traits_type>
        void PrintHelpImpl(Command<CHAR> const* command, std::basic_ostream<CHAR, TSTREAMT>& stream) const;

        class ResultValueViewImpl : public ResultValueView
        {
        public:
            ResultValueViewImpl() = default;

            ResultValueViewImpl(std::basic_string_view<CHAR> str, std::shared_ptr<ResultErrorInfo> errorInfo,
                std::optional<WithIdentity<CHAR>> source, int position)
                : ResultValueView(str, errorInfo, source, position)
            {
            }
        };

        class ResultImpl : public Result
        {
        public:
            ResultImpl()
                : Result()
            {
            }

            using Result::AddCommand;
            using Result::AddMatchedArgument;
            using Result::AddOption;
            using Result::AddSwitch;
            using Result::AddUnmatchedArgument;
            using Result::GetErrorInfo;
        };

        bool m_withImplicitHelpSwitch = true;
        bool m_errorOnUnmatchedArguments = true;
    };

    template <>
    inline void Parser<char>::Result::PrintError(bool tryUseColor) const
    {
        PrintError(std::cout, tryUseColor);
    }

    template <>
    inline void Parser<wchar_t>::Result::PrintError(bool tryUseColor) const
    {
        PrintError(std::wcout, tryUseColor);
    }

    template <typename CHAR>
    template <typename TSTREAMT>
    void Parser<CHAR>::Result::PrintError(std::basic_ostream<CHAR, TSTREAMT>& stream, [[maybe_unused]] bool tryUseColor) const
    {
        if (Result::m_errorInfo->GetError().empty())
        {
            return;
        }

#ifndef _WIN32
        constexpr
#endif
            bool useColor = false;
#ifdef _WIN32
        if (tryUseColor)
        {
            HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD mode;
            if (GetConsoleMode(hStdOut, &mode))
            {
                SetConsoleMode(hStdOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
                if (GetConsoleMode(hStdOut, &mode))
                {
                    if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)
                    {
                        useColor = true;
                    }
                }
            }
        }
#endif
        if (useColor)
        {
            stream << "\x1B[91m\x1B[40m";
        }
        stream << m_errorInfo->GetError();
        if (useColor)
        {
            stream << "\x1B[0m";
        }
        stream << "\n";
    }

    template <typename CHAR>
    template <typename TSTREAMT>
    void Parser<CHAR>::PrintHelp(Command<CHAR> const& command, std::basic_ostream<CHAR, TSTREAMT>& stream) const
    {
        PrintHelpImpl(&command, stream);
    }

    template <typename CHAR>
    template <typename TSTREAMT>
    void Parser<CHAR>::PrintHelp(Result const& result, std::basic_ostream<CHAR, TSTREAMT>& stream) const
    {
        Command<CHAR> const* cmd = nullptr;
        if (!result.Commands().empty())
        {
            WithIdentity<CHAR> cmdId = result.Commands().back();
            for (auto cmdIt = WithCommandContainer<CHAR>::CommandsBegin();
                cmdIt != WithCommandContainer<CHAR>::CommandsEnd(); ++cmdIt)
            {
                if (WithIdentity<CHAR>::Equals(*cmdIt, cmdId))
                {
                    cmd = &*cmdIt;
                    break;
                }
            }
        }
        PrintHelpImpl(cmd, stream);
    }

    template <typename CHAR>
    template <typename TSTREAMT>
    void Parser<CHAR>::PrintHelp(std::basic_ostream<CHAR, TSTREAMT>& stream) const
    {
        PrintHelpImpl(nullptr, stream);
    }

    template <>
    inline void Parser<char>::PrintHelp(Command<char> const& command) const
    {
        PrintHelpImpl(&command, std::cout);
    }

    template <>
    inline void Parser<char>::PrintHelp(Result const& result) const
    {
        PrintHelp(result, std::cout);
    }

    template <>
    inline void Parser<char>::PrintHelp() const
    {
        PrintHelpImpl(nullptr, std::cout);
    }

    template <>
    inline void Parser<wchar_t>::PrintHelp(Command<wchar_t> const& command) const
    {
        PrintHelpImpl(&command, std::wcout);
    }

    template <>
    inline void Parser<wchar_t>::PrintHelp(Result const& result) const
    {
        PrintHelp(result, std::wcout);
    }

    template <>
    inline void Parser<wchar_t>::PrintHelp() const
    {
        PrintHelpImpl(nullptr, std::wcout);
    }

    template <>
    struct Parser<char>::StringConsts
    {
        static constexpr char const nl = '\n';
        static constexpr char const s = ' ';
        static constexpr char const ob = '<';
        static constexpr char const cb = '>';

        static constexpr char const* descriptionCaption = "Description:";
        static constexpr char const* usageCaption = "Usage:";
        static constexpr char const* commandsCaption = "Commands:";
        static constexpr char const* optionsCaption = "Options:";
        static constexpr char const* argumentsCaption = "Arguments:";
        static constexpr char const* command = "[command]";
        static constexpr char const* options = "[options]";

        static constexpr char const* tagRequired = "[required]";
        static constexpr char const* tagOptional = "[optional]";

        static constexpr char const* helpName = "--help";
        static constexpr char const* helpAlias1 = "-h";
        static constexpr char const* helpAlias2 = "/h";
        static constexpr char const* helpAlias3 = "-?";
        static constexpr char const* helpAlias4 = "/?";
        static constexpr char const* helpDescription = "Show help and usage information";

        static constexpr char const* parserStopToken = "--";

        static constexpr char const* errorOptionNoValue = "Value of option expected, but no more arguments: ";
        static constexpr char const* errorUnmatchedArguments = "Unmatched arguments present in command line";
        static constexpr char const* errorRequiredArgumentMissing = "Required argument missing: ";
        static constexpr char const* errorOptionSpecifiedMultipletimes =
            "Option was specified multiple times in the command line: ";

        static constexpr char const* errorParserValueConversion = "Failed to convert value for argument ";
        static constexpr char const* errorGenericParserError = "internal generic error";
        static constexpr char const* errorParserUnexpectedCharAt = "unexpected character at position ";
        static constexpr char const* errorContextSeparator = ": ";
        static constexpr char const* errorMissingInput = "missing expected input";
        static constexpr char const* errorUnexpectedInput = "unexpected input";
        static constexpr char const* errorDataTypeLimit = "value exceeds supported data type limit";

        static inline bool isspace(char c)
        {
            return std::isspace(c);
        }

        static inline char asChar(char c)
        {
            return c;
        }

        template <typename T>
        static inline std::string to_string(T v)
        {
            return std::to_string(v);
        }
    };

    template <>
    struct Parser<wchar_t>::StringConsts
    {
        static constexpr wchar_t const nl = L'\n';
        static constexpr wchar_t const s = L' ';
        static constexpr wchar_t const ob = L'<';
        static constexpr wchar_t const cb = L'>';

        static constexpr wchar_t const* descriptionCaption = L"Description:";
        static constexpr wchar_t const* usageCaption = L"Usage:";
        static constexpr wchar_t const* commandsCaption = L"Commands:";
        static constexpr wchar_t const* optionsCaption = L"Options:";
        static constexpr wchar_t const* argumentsCaption = L"Arguments:";
        static constexpr wchar_t const* command = L"[command]";
        static constexpr wchar_t const* options = L"[options]";

        static constexpr wchar_t const* tagRequired = L"[required]";
        static constexpr wchar_t const* tagOptional = L"[optional]";

        static constexpr wchar_t const* helpName = L"--help";
        static constexpr wchar_t const* helpAlias1 = L"-h";
        static constexpr wchar_t const* helpAlias2 = L"/h";
        static constexpr wchar_t const* helpAlias3 = L"-?";
        static constexpr wchar_t const* helpAlias4 = L"/?";
        static constexpr wchar_t const* helpDescription = L"Show help and usage information";

        static constexpr wchar_t const* parserStopToken = L"--";

        static constexpr wchar_t const* errorOptionNoValue = L"Value of option expected, but no more arguments: ";
        static constexpr wchar_t const* errorUnmatchedArguments = L"Unmatched arguments present in command line";
        static constexpr wchar_t const* errorRequiredArgumentMissing = L"Required argument missing: ";
        static constexpr wchar_t const* errorOptionSpecifiedMultipletimes =
            L"Option was specified multiple times in the command line: ";

        static constexpr wchar_t const* errorParserValueConversion = L"Failed to convert value for argument ";
        static constexpr wchar_t const* errorGenericParserError = L"internal generic error";
        static constexpr wchar_t const* errorParserUnexpectedCharAt = L"unexpected character at position ";
        static constexpr wchar_t const* errorContextSeparator = L": ";
        static constexpr wchar_t const* errorMissingInput = L"missing expected input";
        static constexpr wchar_t const* errorUnexpectedInput = L"unexpected input";
        static constexpr wchar_t const* errorDataTypeLimit = L"value exceeds supported data type limit";

        static inline bool isspace(wchar_t c)
        {
            return std::iswspace(c);
        }

        static inline char asChar(wchar_t c)
        {
            return (static_cast<int>(c) <= 127) ? static_cast<char>(c) : '?';
        }

        template <typename T>
        static inline std::wstring to_string(T v)
        {
            return std::to_wstring(v);
        }
    };

    template <typename CHAR>
    std::basic_string<CHAR> WithNameAndAlias<CHAR>::GetAllNames() const
    {
        std::basic_string<CHAR> names;
        for (Alias<CHAR> const& a : m_names)
        {
            if (!names.empty())
            {
                names += static_cast<CHAR>(',');
                names += static_cast<CHAR>(' ');
            }
            names += a.GetName();
        }
        return names;
    }

    template <typename CHAR>
    template <typename TSTREAMT>
    void Parser<CHAR>::PrintHelpImpl(Command<CHAR> const* command, std::basic_ostream<CHAR, TSTREAMT>& stream) const
    {
        using s = StringConsts;
        using string = std::basic_string<CHAR>;
        using stringPair = std::tuple<string, string>;

        size_t width = 80;
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        {
            width = static_cast<size_t>(csbi.srWindow.Right - csbi.srWindow.Left + 1);
        }
#else
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0)
        {
            width = static_cast<size_t>(w.ws_col);
        }
#endif
        if (width < 30)
        {
            width = 30;
        }
        width--; // reserve last column to host an explicit new line

        WithDescription<CHAR> const* desc = (command == nullptr) ? static_cast<WithDescription<CHAR> const*>(this)
            : static_cast<WithDescription<CHAR> const*>(command);
        if (!desc->GetDescription().empty())
        {
            stream << s::descriptionCaption << s::nl;
            typename string::const_iterator descBegin = desc->GetDescription().cbegin();
            typename string::const_iterator descEnd = desc->GetDescription().cend();
            while (descBegin != descEnd)
            {
                size_t descLen = descEnd - descBegin;
                if (descLen > width - 2)
                {
                    descLen = width - 2;
                    for (size_t i = width - 2; i > width / 4; --i)
                    {
                        if (std::isspace(*(descBegin + i)))
                        {
                            descLen = i;
                            break;
                        }
                    }
                }
                stream << s::s << s::s << std::basic_string_view<CHAR>{&*descBegin, descLen} << s::nl;
                descBegin += descLen;
                while (descBegin != descEnd && std::isspace(*descBegin))
                {
                    descBegin++;
                }
            }
            stream << s::nl;
        }

        std::vector<Command<CHAR> const*> commandChain;
        if (command != nullptr)
        {
            std::function<bool(WithCommandContainer<CHAR> const& cmds)> search;
            search = [command, &search, &commandChain](WithCommandContainer<CHAR> const& cmds)
                {
                    for (auto cmdIt = cmds.CommandsBegin(); cmdIt != cmds.CommandsEnd(); ++cmdIt)
                    {
                        if (WithIdentity<CHAR>::Equals(*command, *cmdIt) ||
                            search(static_cast<WithCommandContainer<CHAR> const&>(*cmdIt)))
                        {
                            commandChain.push_back(&*cmdIt);
                            return true;
                        }
                    }
                    return false;
                };
            search(static_cast<WithCommandContainer<CHAR> const&>(*this));
        }

        std::vector<Option<CHAR> const*> allOptions;
        std::vector<Switch<CHAR> const*> allSwitches;
        std::vector<Argument<CHAR> const*> allArguments;

        auto const addRange = [](auto& vec, auto itBegin, auto itEnd)
            {
                for (auto optIt = itBegin; optIt != itEnd; ++optIt)
                {
                    vec.push_back(&*optIt);
                }
            };
        addRange(allOptions, Parser<CHAR>::OptionsBegin(), Parser<CHAR>::OptionsEnd());
        addRange(allSwitches, Parser<CHAR>::SwitchesBegin(), Parser<CHAR>::SwitchesEnd());
        addRange(allArguments, Parser<CHAR>::ArgumentsBegin(), Parser<CHAR>::ArgumentsEnd());
        for (auto const& c : commandChain)
        {
            addRange(allOptions, c->OptionsBegin(), c->OptionsEnd());
            addRange(allSwitches, c->SwitchesBegin(), c->SwitchesEnd());
            addRange(allArguments, c->ArgumentsBegin(), c->ArgumentsEnd());
        }

        Switch<CHAR> helpSwitch{ s::helpName, s::helpDescription };
        helpSwitch.AddAlias(s::helpAlias1).AddAlias(s::helpAlias2).AddAlias(s::helpAlias3).AddAlias(s::helpAlias4);
        if (m_withImplicitHelpSwitch)
        {
            allSwitches.push_back(&helpSwitch);
        }

        allOptions.erase(std::remove_if(allOptions.begin(), allOptions.end(),
            [](Option<CHAR> const* o) { return o->DoesHideFromHelp(); }),
            allOptions.end());
        allSwitches.erase(std::remove_if(allSwitches.begin(), allSwitches.end(),
            [](Switch<CHAR> const* o) { return o->DoesHideFromHelp(); }),
            allSwitches.end());

        size_t x = 0;

        stream << s::usageCaption << s::nl << s::s << s::s << Parser<CHAR>::GetName();
        x = 2 + Parser<CHAR>::GetName().size();
        auto optionalLineBreak = [&x, &width, &stream](size_t l)
            {
                if (x + l >= width)
                {
                    stream << s::nl << s::s << s::s << s::s;
                    x = l + 3;
                }
                else
                {
                    x += l;
                }
            };
        for (auto c = commandChain.crbegin(); c != commandChain.crend(); ++c)
        {
            auto const& n = (*c)->NameAliasBegin()->GetName();
            optionalLineBreak(n.size() + 1);
            stream << s::s << n;
        }
        WithCommandContainer<CHAR> const* cmds = (command == nullptr)
            ? static_cast<WithCommandContainer<CHAR> const*>(this)
            : static_cast<WithCommandContainer<CHAR> const*>(command);

        int cmdsCnt = 0;
        for (auto cmdsIt = cmds->CommandsBegin(); cmdsIt != cmds->CommandsEnd(); ++cmdsIt)
        {
            if (cmdsIt->DoesHideFromHelp())
            {
                continue;
            }
            cmdsCnt++;
        }
        if (cmdsCnt > 0)
        {
            optionalLineBreak(cexprStrLen(s::command) + 1);
            stream << s::s << s::command;
        }

        for (Argument<CHAR> const* arg : allArguments)
        {
            if (!arg->IsRequired())
            {
                continue;
            }
            auto const& n = arg->GetName();
            optionalLineBreak(n.size() + 3);
            stream << s::s << s::ob << n << s::cb;
        }

        if (!allOptions.empty() || !allSwitches.empty())
        {
            optionalLineBreak(cexprStrLen(s::options) + 1);
            stream << s::s << s::options;
        }
        stream << s::nl << s::nl;

        std::vector<stringPair> docu;
        auto printAndCountSpaces = [&stream](size_t& counter, size_t count)
            {
                if (count <= 0)
                {
                    return;
                }
                stream << std::setfill(s::s) << std::setw(count) << s::s;
                counter += count;
            };
        auto formatDocuTable = [&docu, &width, &stream, &printAndCountSpaces]()
            {
                size_t c1w = 0;
                for (stringPair const& sp : docu)
                {
                    size_t s = std::get<0>(sp).size();
                    if (c1w < s)
                    {
                        c1w = s;
                    }
                }
                if (c1w >= width / 4)
                {
                    c1w = (width / 4) - 1;
                }
                size_t maxc1w = width / 3;

                for (stringPair const& sp : docu)
                {
                    std::vector<std::basic_string_view<CHAR>> nameLines;

                    typename string::const_iterator nameBegin = std::get<0>(sp).cbegin();
                    typename string::const_iterator nameEnd = std::get<0>(sp).cend();

                    while (nameBegin != nameEnd)
                    {
                        typename string::const_iterator nameNext = nameEnd;

                        size_t nameLen = static_cast<size_t>(nameNext - nameBegin);
                        if (nameLen > c1w)
                        {
                            if (nameLen > maxc1w)
                            {
                                // wrap name text
                                bool found = false;
                                for (typename string::const_iterator it = nameBegin + c1w; it != nameBegin; --it)
                                {
                                    if (s::isspace(*it))
                                    {
                                        nameNext = it;
                                        found = true;
                                        break;
                                    }
                                }

                                if (!found)
                                {
                                    for (typename string::const_iterator it = nameBegin + maxc1w; it != nameBegin; --it)
                                    {
                                        if (s::isspace(*it))
                                        {
                                            nameNext = it;
                                            found = true;
                                            break;
                                        }
                                    }
                                }

                                if (!found)
                                {
                                    nameNext = nameBegin + c1w;
                                }
                            }
                            // else, name is only slightly too large. Do hanging text without wrap
                        }
                        for (typename string::const_iterator it = nameBegin; it != nameNext; ++it)
                        {
                            if (*it == s::nl)
                            {
                                nameNext = it;
                                break;
                            }
                        }

                        nameLines.push_back(
                            std::basic_string_view<CHAR>{&*nameBegin, static_cast<size_t>(nameNext - nameBegin)});

                        if (nameNext != nameEnd && *nameNext == s::nl)
                        {
                            nameNext++;
                        }
                        else
                        {
                            while (nameNext != nameEnd && std::isspace(*nameNext))
                            {
                                nameNext++;
                            }
                        }
                        nameBegin = nameNext;
                    }

                    size_t c1ew = c1w;
                    for (auto const& s : nameLines)
                    {
                        if (c1ew < s.size())
                        {
                            c1ew = s.size();
                        }
                    }

                    auto nextNameLine = nameLines.cbegin();
                    auto descBegin = std::get<1>(sp).cbegin();
                    auto descEnd = std::get<1>(sp).cend();
                    while (nextNameLine != nameLines.cend() || descBegin != descEnd)
                    {
                        size_t x = 0;
                        printAndCountSpaces(x, 2);
                        if (nextNameLine != nameLines.cend())
                        {
                            auto const& name = *nextNameLine;
                            x += name.size();
                            stream << name;
                            nextNameLine++;
                        };
                        printAndCountSpaces(x, c1ew + 2 - x + 2);

                        if (descBegin != descEnd)
                        {

                            typename string::const_iterator descNext = descEnd;
                            size_t descLen = descNext - descBegin;
                            if (descLen > width - x)
                            {
                                descNext = descBegin + (width - x);
                                for (size_t i = width - x; i > (width - x) / 2; --i)
                                {
                                    if (std::isspace(*(descBegin + i)))
                                    {
                                        descNext = descBegin + i;
                                        break;
                                    }
                                }
                            }

                            stream << std::basic_string_view{ &*descBegin, static_cast<size_t>(descNext - descBegin) };

                            while (descNext != descEnd && std::isspace(*descNext))
                            {
                                descNext++;
                            }
                            descBegin = descNext;
                        }
                        stream << s::nl;
                    }
                }
            };

        if (!allArguments.empty())
        {
            stream << s::argumentsCaption << s::nl;
            docu.clear();
            for (Argument<CHAR> const* arg : allArguments)
            {
                string desc2 = (arg->IsRequired() ? s::tagRequired : s::tagOptional);
                desc2 += s::s;
                desc2 += arg->GetDescription();
                docu.push_back({ arg->GetName(), desc2 });
            }

            formatDocuTable();

            stream << s::nl;
        }

        if (!allOptions.empty() || !allSwitches.empty())
        {
            stream << s::optionsCaption << s::nl;
            docu.clear();
            for (Option<CHAR> const* opt : allOptions)
            {
                docu.push_back({ opt->GetAllNames() + s::nl + s::s + s::s + s::ob + opt->GetArgumentName() + s::cb,
                                opt->GetDescription() });
            }
            for (Switch<CHAR> const* sw : allSwitches)
            {
                docu.push_back({ sw->GetAllNames(), sw->GetDescription() });
            }

            formatDocuTable();

            stream << s::nl;
        }

        if (cmdsCnt > 0)
        {
            stream << s::commandsCaption << s::nl;
            docu.clear();
            for (auto cmdIt = cmds->CommandsBegin(); cmdIt != cmds->CommandsEnd(); ++cmdIt)
            {
                if (cmdIt->DoesHideFromHelp())
                {
                    continue;
                }
                docu.push_back({ cmdIt->GetAllNames(), cmdIt->GetDescription() });
            }

            formatDocuTable();

            stream << s::nl;
        }
    }

    template <typename CHAR>
    typename Parser<CHAR>::Result Parser<CHAR>::Parse(int argc, const CHAR* const* argv,
        bool skipFirstArg /* = true */) const
    {
        using s = StringConsts;

        ResultImpl res{};

        std::vector<Command<CHAR> const*> allCommands;
        std::vector<Option<CHAR> const*> allOptions;
        std::vector<Switch<CHAR> const*> allSwitches;
        std::vector<Argument<CHAR> const*> allArguments;
        auto const addRange = [](auto& vec, auto itBegin, auto itEnd)
            {
                for (auto optIt = itBegin; optIt != itEnd; ++optIt)
                {
                    vec.push_back(&*optIt);
                }
            };

        Switch<CHAR> helpSwitch{ s::helpName, s::helpDescription };
        helpSwitch.AddAlias(s::helpAlias1).AddAlias(s::helpAlias2).AddAlias(s::helpAlias3).AddAlias(s::helpAlias4);
        if (m_withImplicitHelpSwitch)
        {
            allSwitches.push_back(&helpSwitch);
        }

        addRange(allCommands, Parser<CHAR>::CommandsBegin(), Parser<CHAR>::CommandsEnd());
        addRange(allOptions, Parser<CHAR>::OptionsBegin(), Parser<CHAR>::OptionsEnd());
        addRange(allSwitches, Parser<CHAR>::SwitchesBegin(), Parser<CHAR>::SwitchesEnd());
        addRange(allArguments, Parser<CHAR>::ArgumentsBegin(), Parser<CHAR>::ArgumentsEnd());

        bool errorOnUnmatchedArguments = Parser<CHAR>::IsSetErrorOnUnmatchedArguments();

        Option<CHAR> const* pendingOption = nullptr;

        for (int argi = skipFirstArg ? 1 : 0; argi < argc; ++argi)
        {
            const std::basic_string_view<CHAR> arg{ argv[argi] };
            bool handled = false;

            if (arg == s::parserStopToken)
            {
                for (argi++; argi < argc; ++argi)
                {
                    res.AddUnmatchedArgument(ResultValueViewImpl{ std::basic_string_view<CHAR>{argv[argi]},
                                                                 res.GetErrorInfo(), std::nullopt, argi });
                }
                break;
            }

            if (pendingOption != nullptr)
            {
                res.AddOption(ResultValueViewImpl{ arg, res.GetErrorInfo(), *pendingOption, argi });
                handled = true;
                pendingOption = nullptr;
            }
            if (handled)
            {
                continue;
            }

            for (Command<CHAR> const* cmd : allCommands)
            {
                if (cmd->IsMatch(arg))
                {
                    allCommands.clear();
                    addRange(allCommands, cmd->CommandsBegin(), cmd->CommandsEnd());

                    addRange(allOptions, cmd->OptionsBegin(), cmd->OptionsEnd());
                    addRange(allSwitches, cmd->SwitchesBegin(), cmd->SwitchesEnd());
                    addRange(allArguments, cmd->ArgumentsBegin(), cmd->ArgumentsEnd());

                    switch (cmd->GetSetErrorOnUnmatchedArguments())
                    {
                    case Command<CHAR>::OnUnmatchedArguments::SetError:
                        errorOnUnmatchedArguments = true;
                        break;
                    case Command<CHAR>::OnUnmatchedArguments::NoError:
                        errorOnUnmatchedArguments = false;
                        break;
                    case Command<CHAR>::OnUnmatchedArguments::Keep:
                        // no change
                        break;
                    default:
                        res.SetError(s::errorGenericParserError);
                        return res;
                    }

                    res.AddCommand(*cmd);
                    handled = true;
                    break;
                }
            }
            if (handled)
            {
                continue;
            }

            for (Option<CHAR> const* opt : allOptions)
            {
                if (opt->IsMatch(arg))
                {
                    pendingOption = opt;
                    handled = true;
                    break;
                }
                std::basic_string_view<CHAR> valueStr;
                if (opt->IsMatchWithValue(arg, valueStr))
                {
                    handled = true;
                    res.AddOption(ResultValueViewImpl{ valueStr, res.GetErrorInfo(), *opt, argi });
                    break;
                }
            }
            if (handled)
            {
                continue;
            }
            for (Switch<CHAR> const* swt : allSwitches)
            {
                if (swt->IsMatch(arg))
                {
                    handled = true;
                    if (WithIdentity<CHAR>::Equals(*swt, helpSwitch))
                    {
                        res.GetErrorInfo()->SetShouldShowHelp();
                    }
                    else
                    {
                        res.AddSwitch(*swt);
                    }
                    break;
                }
            }
            if (handled)
            {
                continue;
            }

            if (!allArguments.empty())
            {
                Argument<CHAR> const* ma = allArguments.front();
                res.AddMatchedArgument(ResultValueViewImpl{ arg, res.GetErrorInfo(), *ma, argi });
                handled = true;
                allArguments.erase(allArguments.begin());
            }
            if (handled)
            {
                continue;
            }

            res.AddUnmatchedArgument(ResultValueViewImpl{ arg, res.GetErrorInfo(), std::nullopt, argi });
        }

        Argument<CHAR> const* missingRequiredArgument = nullptr;
        for (Argument<CHAR> const* ma : allArguments)
        {
            if (ma->IsRequired())
            {
                missingRequiredArgument = ma;
                break;
            }
        }

        if (pendingOption != nullptr)
        {
            std::basic_string<CHAR> msg{ s::errorOptionNoValue };
            msg += pendingOption->NameAliasBegin()->GetName();
            res.SetError(msg);
        }
        else if (errorOnUnmatchedArguments && res.HasUnmatchedArguments())
        {
            res.SetError(s::errorUnmatchedArguments);
        }
        else if (missingRequiredArgument != nullptr)
        {
            std::basic_string<CHAR> msg{ s::errorRequiredArgumentMissing };
            msg += missingRequiredArgument->GetName();
            res.SetError(msg);
        }
        else
        {
            res.GetErrorInfo()->SetSuccess();
        }

        return res;
    }

    template <>
    inline void Parser<char>::PrintErrorAndHelpIfNeeded(Result const& result) const
    {
        Parser<char>::PrintErrorAndHelpIfNeeded(result, std::cout);
    }

    template <>
    inline void Parser<wchar_t>::PrintErrorAndHelpIfNeeded(Result const& result) const
    {
        Parser<wchar_t>::PrintErrorAndHelpIfNeeded(result, std::wcout);
    }

    template <typename CHAR>
    template <typename TSTREAMT>
    void Parser<CHAR>::PrintErrorAndHelpIfNeeded(Result const& result, std::basic_ostream<CHAR, TSTREAMT>& stream) const
    {
        if (!result.IsSuccess())
        {
            result.PrintError(stream);
            stream << StringConsts::nl;
        }
        if (result.ShouldShowHelp())
        {
            Parser<CHAR>::PrintHelp(result, stream);
        }
    }

    template <typename CHAR>
    std::optional<long long> Parser<CHAR>::ResultValueView::AsInteger(bool errorWhenTypeParsingFails) const
    {
        long long limit = (std::numeric_limits<long long>::max)() / 10;

        using s = StringConsts;
        long long v = 0;
        long long base = 10;
        bool neg = false;

        enum class State
        {
            SignPrefixOrValue = 0,
            PrefixOrValue = 1,
            Value = 2
        };
        State state = State::SignPrefixOrValue;

        auto strRange = ResultValueView::GetStringTrimmed();

        for (auto strIt = strRange.first; strIt != strRange.second; ++strIt)
        {
            char c = s::asChar(*strIt);
            switch (state)
            {
            case State::SignPrefixOrValue:
                if (c == '+')
                {
                    state = State::PrefixOrValue;
                    continue;
                }
                if (c == '-')
                {
                    state = State::PrefixOrValue;
                    neg = true;
                    continue;
                }
                [[fallthrough]];
            case State::PrefixOrValue:
                if (c == 'x' || c == 'X')
                {
                    base = 16;
                    limit = (std::numeric_limits<long long>::max)() / 16;
                    state = State::Value;
                    continue;
                }
                if (c == 'o' || c == 'O')
                {
                    base = 8;
                    limit = (std::numeric_limits<long long>::max)() / 8;
                    state = State::Value;
                    continue;
                }
                if (c == 'b' || c == 'B')
                {
                    base = 2;
                    limit = (std::numeric_limits<long long>::max)() / 2;
                    state = State::Value;
                    continue;
                }
                [[fallthrough]];
            case State::Value:
                if (c >= '0' && c <= '1')
                {
                    if (v >= limit)
                    {
                        if (errorWhenTypeParsingFails)
                        {
                            std::basic_string<CHAR> msg{ s::errorParserValueConversion };
                            msg += s::to_string(ResultValueView::GetPosition());
                            msg += s::errorContextSeparator;
                            msg += s::errorDataTypeLimit;
                            m_errorInfo->SetError(msg);
                        }
                        return std::nullopt;
                    }
                    v = v * base + static_cast<int>(c - '0');
                    state = State::Value;
                    continue;
                }
                if (base >= 8 && c >= '2' && c <= '7')
                {
                    if (v >= limit)
                    {
                        if (errorWhenTypeParsingFails)
                        {
                            std::basic_string<CHAR> msg{ s::errorParserValueConversion };
                            msg += s::to_string(ResultValueView::GetPosition());
                            msg += s::errorContextSeparator;
                            msg += s::errorDataTypeLimit;
                            m_errorInfo->SetError(msg);
                        }
                        return std::nullopt;
                    }
                    v = v * base + static_cast<int>(c - '0');
                    state = State::Value;
                    continue;
                }
                if (base >= 10 && c >= '8' && c <= '9')
                {
                    if (v >= limit)
                    {
                        if (errorWhenTypeParsingFails)
                        {
                            std::basic_string<CHAR> msg{ s::errorParserValueConversion };
                            msg += s::to_string(ResultValueView::GetPosition());
                            msg += s::errorContextSeparator;
                            msg += s::errorDataTypeLimit;
                            m_errorInfo->SetError(msg);
                        }
                        return std::nullopt;
                    }
                    v = v * base + static_cast<int>(c - '0');
                    state = State::Value;
                    continue;
                }
                if (base == 16 && (c >= 'a' && c <= 'f'))
                {
                    if (v >= limit)
                    {
                        if (errorWhenTypeParsingFails)
                        {
                            std::basic_string<CHAR> msg{ s::errorParserValueConversion };
                            msg += s::to_string(ResultValueView::GetPosition());
                            msg += s::errorContextSeparator;
                            msg += s::errorDataTypeLimit;
                            m_errorInfo->SetError(msg);
                        }
                        return std::nullopt;
                    }
                    v = v * base + (10 + static_cast<int>(c - 'a'));
                    state = State::Value;
                    continue;
                }
                if (base == 16 && (c >= 'A' && c <= 'F'))
                {
                    if (v >= limit)
                    {
                        if (errorWhenTypeParsingFails)
                        {
                            std::basic_string<CHAR> msg{ s::errorParserValueConversion };
                            msg += s::to_string(ResultValueView::GetPosition());
                            msg += s::errorContextSeparator;
                            msg += s::errorDataTypeLimit;
                            m_errorInfo->SetError(msg);
                        }
                        return std::nullopt;
                    }
                    v = v * base + (10 + static_cast<int>(c - 'A'));
                    state = State::Value;
                    continue;
                }
                break;

            default:
                if (errorWhenTypeParsingFails)
                {
                    std::basic_string<CHAR> msg{ s::errorParserValueConversion };
                    msg += s::to_string(ResultValueView::GetPosition());
                    msg += s::errorContextSeparator;
                    msg += s::errorGenericParserError;
                    m_errorInfo->SetError(msg);
                }
                return std::nullopt;
            }

            if (errorWhenTypeParsingFails)
            {
                std::basic_string<CHAR> msg{ s::errorParserValueConversion };
                msg += s::to_string(ResultValueView::GetPosition());
                msg += s::errorContextSeparator;
                msg += s::errorParserUnexpectedCharAt;
                msg += s::to_string(1 + strIt - strRange.first);
                m_errorInfo->SetError(msg);
            }
            return std::nullopt;
        }

        if (state != State::Value)
        {
            if (errorWhenTypeParsingFails)
            {
                std::basic_string<CHAR> msg{ s::errorParserValueConversion };
                msg += s::to_string(ResultValueView::GetPosition());
                msg += s::errorContextSeparator;
                msg += s::errorMissingInput;
                m_errorInfo->SetError(msg);
            }
            return std::nullopt;
        }

        if (neg)
        {
            v = -v;
        }
        return v;
    }

    template <typename CHAR>
    std::optional<bool> Parser<CHAR>::ResultValueView::AsBool(bool errorWhenTypeParsingFails) const
    {
        auto strRange = ResultValueView::GetStringTrimmed();

        if (strRange.first == strRange.second)
        {
            if (errorWhenTypeParsingFails)
            {
                std::basic_string<CHAR> msg{ StringConsts::errorParserValueConversion };
                msg += StringConsts::to_string(ResultValueView::GetPosition());
                msg += StringConsts::errorContextSeparator;
                msg += StringConsts::errorMissingInput;
                m_errorInfo->SetError(msg);
            }
            return std::nullopt;
        }

        if (strRange.second - strRange.first <= 5)
        {
            // possibly a key word
            int len = static_cast<int>(strRange.second - strRange.first);
            char s[5]{ 0, 0, 0, 0, 0 };
            for (int i = 0; i < len; ++i)
            {
                s[i] = static_cast<char>(std::tolower(StringConsts::asChar(*(strRange.first + i))));
            }

            if (std::memcmp(s, "true", 4) == 0 || std::memcmp(s, "t", 1) == 0 || std::memcmp(s, "on", 2) == 0 ||
                std::memcmp(s, "yes", 3) == 0 || std::memcmp(s, "y", 1) == 0)
            {
                return true;
            }
            if (std::memcmp(s, "false", 5) == 0 || std::memcmp(s, "f", 1) == 0 || std::memcmp(s, "off", 3) == 0 ||
                std::memcmp(s, "no", 2) == 0 || std::memcmp(s, "n", 1) == 0)
            {
                return false;
            }
        }

        auto intVal = ResultValueView::AsInteger(errorWhenTypeParsingFails);
        if (intVal.has_value())
        {
            return intVal.value() != 0;
        }

        if (errorWhenTypeParsingFails)
        {
            std::basic_string<CHAR> msg{ StringConsts::errorParserValueConversion };
            msg += StringConsts::to_string(ResultValueView::GetPosition());
            msg += StringConsts::errorContextSeparator;
            msg += StringConsts::errorUnexpectedInput;
            m_errorInfo->SetError(msg);
        }

        return std::nullopt;
    }

    template <typename CHAR>
    std::optional<double> Parser<CHAR>::ResultValueView::AsDouble(bool errorWhenTypeParsingFails) const
    {
        using s = StringConsts;

        constexpr const long long limit = (std::numeric_limits<long long>::max)() / 10;

        bool negVal = false;
        long long fullVal = 0;
        long long fracVal = 0;
        long long fracWidth = 1;
        bool negExp = false;
        long expVal = 0;

        // [+-]?[0..9]*\.?[0..9]*([eE][+-]?[0..9]+)?
        // 0    1         2           3    4
        enum class State
        {
            SignOrFullVal = 0,
            FullVal = 1,
            FracVal = 2,
            ExpSignOrVal = 3,
            ExpVal = 4
        };
        State state = State::SignOrFullVal;

        auto strRange = ResultValueView::GetStringTrimmed();

        for (auto strIt = strRange.first; strIt != strRange.second; ++strIt)
        {
            char c = s::asChar(*strIt);
            switch (state)
            {
            case State::SignOrFullVal:
                if (c == '+')
                {
                    state = State::FullVal;
                    continue;
                }
                if (c == '-')
                {
                    negVal = true;
                    state = State::FullVal;
                    continue;
                }
                [[fallthrough]];
            case State::FullVal:
                if (c >= '0' && c <= '9')
                {
                    if (fullVal >= limit)
                    {
                        if (errorWhenTypeParsingFails)
                        {
                            std::basic_string<CHAR> msg{ s::errorParserValueConversion };
                            msg += s::to_string(ResultValueView::GetPosition());
                            msg += s::errorContextSeparator;
                            msg += s::errorDataTypeLimit;
                            m_errorInfo->SetError(msg);
                        }
                        return std::nullopt;
                    }
                    state = State::FullVal;
                    fullVal *= 10;
                    fullVal += c - '0';
                    continue;
                }
                if (c == '.')
                {
                    state = State::FracVal;
                    continue;
                }
                if (c == 'e' || c == 'E')
                {
                    state = State::ExpSignOrVal;
                    continue;
                }
                break;
            case State::FracVal:
                if (c >= '0' && c <= '9')
                {
                    if (fracVal < limit)
                    {
                        fracWidth *= 10;
                        fracVal *= 10;
                        fracVal += c - '0';
                    }
                    continue;
                }
                if (c == 'e' || c == 'E')
                {
                    state = State::ExpSignOrVal;
                    continue;
                }
                break;
            case State::ExpSignOrVal:
                if (c == '+')
                {
                    state = State::ExpVal;
                    continue;
                }
                if (c == '-')
                {
                    negExp = true;
                    state = State::ExpVal;
                    continue;
                }
                [[fallthrough]];
            case State::ExpVal:
                if (c >= '0' && c <= '9')
                {
                    if (expVal >= limit)
                    {
                        if (errorWhenTypeParsingFails)
                        {
                            std::basic_string<CHAR> msg{ s::errorParserValueConversion };
                            msg += s::to_string(ResultValueView::GetPosition());
                            msg += s::errorContextSeparator;
                            msg += s::errorDataTypeLimit;
                            m_errorInfo->SetError(msg);
                        }
                        return std::nullopt;
                    }
                    expVal *= 10;
                    expVal += c - '0';
                    continue;
                }
                break;

            default:
                if (errorWhenTypeParsingFails)
                {
                    std::basic_string<CHAR> msg{ s::errorParserValueConversion };
                    msg += s::to_string(ResultValueView::GetPosition());
                    msg += s::errorContextSeparator;
                    msg += s::errorGenericParserError;
                    m_errorInfo->SetError(msg);
                }
                return std::nullopt;
            }

            if (errorWhenTypeParsingFails)
            {
                std::basic_string<CHAR> msg{ s::errorParserValueConversion };
                msg += s::to_string(ResultValueView::GetPosition());
                msg += s::errorContextSeparator;
                msg += s::errorParserUnexpectedCharAt;
                msg += s::to_string(1 + strIt - strRange.first);
                m_errorInfo->SetError(msg);
            }
            return std::nullopt;
        }

        if (state < State::FullVal || state > State::ExpVal)
        {
            if (errorWhenTypeParsingFails)
            {
                std::basic_string<CHAR> msg{ s::errorParserValueConversion };
                msg += s::to_string(ResultValueView::GetPosition());
                msg += s::errorContextSeparator;
                msg += s::errorMissingInput;
                m_errorInfo->SetError(msg);
            }
            return std::nullopt;
        }

        double d = static_cast<double>(fullVal) + static_cast<double>(fracVal) / static_cast<double>(fracWidth);
        const double e = std::pow(10.0, static_cast<double>(negExp ? -expVal : expVal));
        d *= e;
        if (negVal)
        {
            d = -d;
        }

        return d;
    }

} // namespace yaclap