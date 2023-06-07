#include <functional>
#include <limits>
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QValidator>
#include <QWidget>
#include "common/common_types.h"
#include "common/settings.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/shared_translation.h"
#include "yuzu/configuration/shared_widget.h"

namespace ConfigurationShared {

static int restore_button_count = 0;

QPushButton* Widget::CreateRestoreGlobalButton(bool using_global, QWidget* parent) {
    restore_button_count++;

    QStyle* style = parent->style();
    QIcon* icon = new QIcon(style->standardIcon(QStyle::SP_LineEditClearButton));
    QPushButton* restore_button = new QPushButton(*icon, QStringLiteral(""), parent);
    restore_button->setObjectName(QStringLiteral("RestoreButton%1").arg(restore_button_count));
    restore_button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    // Workaround for dark theme causing min-width to be much larger than 0
    restore_button->setStyleSheet(
        QStringLiteral("QAbstractButton#%1 { min-width: 0px }").arg(restore_button->objectName()));

    QSizePolicy sp_retain = restore_button->sizePolicy();
    sp_retain.setRetainSizeWhenHidden(true);
    restore_button->setSizePolicy(sp_retain);

    restore_button->setEnabled(!using_global);
    restore_button->setVisible(!using_global);

    return restore_button;
}

QLabel* Widget::CreateLabel(const QString& text) {
    QLabel* qt_label = new QLabel(text, this->parent);
    qt_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    return qt_label;
}

QWidget* Widget::CreateCheckBox(Settings::BasicSetting* bool_setting, const QString& label,
                                std::function<std::string()>& serializer,
                                std::function<void()>& restore_func,
                                const std::function<void()>& touch) {
    checkbox = new QCheckBox(label, this);
    checkbox->setCheckState(bool_setting->ToString() == "true" ? Qt::CheckState::Checked
                                                               : Qt::CheckState::Unchecked);
    checkbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    if (!bool_setting->Save() && !Settings::IsConfiguringGlobal() && runtime_lock) {
        checkbox->setEnabled(false);
    }

    serializer = [this]() {
        return checkbox->checkState() == Qt::CheckState::Checked ? "true" : "false";
    };

    if (!Settings::IsConfiguringGlobal()) {
        restore_func = [this, bool_setting]() {
            checkbox->setCheckState(bool_setting->ToStringGlobal() == "true" ? Qt::Checked
                                                                             : Qt::Unchecked);
        };

        QObject::connect(checkbox, &QCheckBox::clicked, [touch]() { touch(); });
    }

    return checkbox;
}

QWidget* Widget::CreateCombobox(std::function<std::string()>& serializer,
                                std::function<void()>& restore_func,
                                const std::function<void()>& touch) {
    const auto type = setting.TypeId();

    combobox = new QComboBox(this);
    combobox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    const ComboboxTranslations* enumeration{nullptr};
    if (combobox_enumerations.contains(type)) {
        enumeration = &combobox_enumerations.at(type);
        for (const auto& [id, name] : *enumeration) {
            combobox->addItem(name);
        }
    } else {
        return combobox;
    }

    const auto find_index = [=](u32 value) -> int {
        for (u32 i = 0; i < enumeration->size(); i++) {
            if (enumeration->at(i).first == value) {
                return i;
            }
        }
        return -1;
    };

    const u32 setting_value = std::stoi(setting.ToString());
    combobox->setCurrentIndex(find_index(setting_value));

    serializer = [this, enumeration]() {
        int current = combobox->currentIndex();
        return std::to_string(enumeration->at(current).first);
    };

    if (!Settings::IsConfiguringGlobal()) {
        restore_func = [this, find_index]() {
            const u32 global_value = std::stoi(setting.ToStringGlobal());
            combobox->setCurrentIndex(find_index(global_value));
        };

        QObject::connect(combobox, QOverload<int>::of(&QComboBox::activated),
                         [touch]() { touch(); });
    }

    return combobox;
}

QWidget* Widget::CreateLineEdit(std::function<std::string()>& serializer,
                                std::function<void()>& restore_func,
                                const std::function<void()>& touch, bool managed) {
    const QString text = QString::fromStdString(setting.ToString());
    line_edit = new QLineEdit(this);
    line_edit->setText(text);

    serializer = [this]() { return line_edit->text().toStdString(); };

    if (!managed) {
        return line_edit;
    }

    if (!Settings::IsConfiguringGlobal()) {
        restore_func = [this]() {
            line_edit->setText(QString::fromStdString(setting.ToStringGlobal()));
        };

        QObject::connect(line_edit, &QLineEdit::textChanged, [touch]() { touch(); });
    }

    return line_edit;
}

QWidget* Widget::CreateSlider(bool reversed, float multiplier, const QString& format,
                              std::function<std::string()>& serializer,
                              std::function<void()>& restore_func,
                              const std::function<void()>& touch) {
    QWidget* container = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(container);

    slider = new QSlider(Qt::Horizontal, this);
    QLabel* feedback = new QLabel(this);

    layout->addWidget(slider);
    layout->addWidget(feedback);

    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layout->setContentsMargins(0, 0, 0, 0);

    int max_val = std::stoi(setting.MaxVal());

    const QString use_format = format == QStringLiteral("") ? QStringLiteral("%1") : format;

    QObject::connect(slider, &QAbstractSlider::valueChanged, [=](int value) {
        int present = (reversed ? max_val - value : value) * multiplier + 0.5f;
        feedback->setText(use_format.arg(QVariant::fromValue(present).value<QString>()));
    });

    slider->setMinimum(std::stoi(setting.MinVal()));
    slider->setMaximum(max_val);
    slider->setValue(std::stoi(setting.ToString()));

    slider->setInvertedAppearance(reversed);

    serializer = [this]() { return std::to_string(slider->value()); };

    if (!Settings::IsConfiguringGlobal()) {
        restore_func = [this]() { slider->setValue(std::stoi(setting.ToStringGlobal())); };

        QObject::connect(slider, &QAbstractSlider::sliderReleased, [touch]() { touch(); });
    }

    return container;
}

QWidget* Widget::CreateSpinBox(const QString& suffix, std::function<std::string()>& serializer,
                               std::function<void()>& restore_func,
                               const std::function<void()>& touch) {
    const int min_val = std::stoi(setting.MinVal());
    const int max_val = std::stoi(setting.MaxVal());
    const int default_val = std::stoi(setting.ToString());

    spinbox = new QSpinBox(this);
    spinbox->setRange(min_val, max_val);
    spinbox->setValue(default_val);
    spinbox->setSuffix(suffix);
    spinbox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    serializer = [this]() { return std::to_string(spinbox->value()); };

    if (!Settings::IsConfiguringGlobal()) {
        restore_func = [this]() { spinbox->setValue(std::stoi(setting.ToStringGlobal())); };

        QObject::connect(spinbox, QOverload<int>::of(&QSpinBox::valueChanged), [this, touch]() {
            if (spinbox->value() != std::stoi(setting.ToStringGlobal())) {
                touch();
            }
        });
    }

    return spinbox;
}

QWidget* Widget::CreateHexEdit(std::function<std::string()>& serializer,
                               std::function<void()>& restore_func,
                               const std::function<void()>& touch) {
    auto* data_component = CreateLineEdit(serializer, restore_func, touch, false);
    if (data_component == nullptr) {
        return nullptr;
    }

    auto to_hex = [=](const std::string& input) {
        return QString::fromStdString(fmt::format("{:08x}", std::stoi(input)));
    };

    QRegExpValidator* regex =
        new QRegExpValidator{QRegExp{QStringLiteral("^[0-9a-fA-F]{0,8}$")}, line_edit};

    const QString default_val = to_hex(setting.ToString());

    line_edit->setText(default_val);
    line_edit->setMaxLength(8);
    line_edit->setValidator(regex);

    auto hex_to_dec = [=]() -> std::string {
        return std::to_string(std::stoul(line_edit->text().toStdString(), nullptr, 16));
    };

    serializer = [hex_to_dec]() { return hex_to_dec(); };

    if (!Settings::IsConfiguringGlobal()) {
        restore_func = [this, to_hex]() { line_edit->setText(to_hex(setting.ToStringGlobal())); };

        QObject::connect(line_edit, &QLineEdit::textChanged, [touch]() { touch(); });
    }

    return line_edit;
}

QWidget* Widget::CreateDateTimeEdit(bool disabled, bool restrict,
                                    std::function<std::string()>& serializer,
                                    std::function<void()>& restore_func,
                                    const std::function<void()>& touch) {
    const long long current_time = QDateTime::currentSecsSinceEpoch();
    const s64 the_time = disabled ? current_time : std::stoll(setting.ToString());
    const auto default_val = QDateTime::fromSecsSinceEpoch(the_time);

    date_time_edit = new QDateTimeEdit(this);
    date_time_edit->setDateTime(default_val);
    date_time_edit->setMinimumDateTime(QDateTime::fromSecsSinceEpoch(0));
    date_time_edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    serializer = [this]() { return std::to_string(date_time_edit->dateTime().toSecsSinceEpoch()); };

    if (!Settings::IsConfiguringGlobal()) {
        auto get_clear_val = [=]() {
            return QDateTime::fromSecsSinceEpoch([=]() {
                if (restrict && checkbox->checkState() == Qt::Checked) {
                    return std::stoll(setting.ToStringGlobal());
                }
                return current_time;
            }());
        };

        restore_func = [=]() { date_time_edit->setDateTime(get_clear_val()); };

        QObject::connect(date_time_edit, &QDateTimeEdit::editingFinished, [=]() {
            if (date_time_edit->dateTime() != get_clear_val()) {
                touch();
            }
        });
    }

    return date_time_edit;
}

void Widget::SetupComponent(const QString& label, std::function<void()>& load_func, bool managed,
                            RequestType request, float multiplier,
                            Settings::BasicSetting* other_setting, const QString& string) {
    created = true;
    const auto type = setting.TypeId();

    QLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    const bool require_checkbox =
        other_setting != nullptr && other_setting->TypeId() == typeid(bool);

    if (other_setting != nullptr && other_setting->TypeId() != typeid(bool)) {
        LOG_WARNING(Frontend,
                    "Extra setting specified but is not bool, refusing to create checkbox for it.");
    }

    std::function<std::string()> checkbox_serializer = []() -> std::string { return {}; };
    std::function<void()> checkbox_restore_func = []() {};

    std::function<void()> touch = []() {};
    std::function<std::string()> serializer = []() -> std::string { return {}; };
    std::function<void()> restore_func = []() {};

    QWidget* data_component{nullptr};

    if (!Settings::IsConfiguringGlobal() && managed) {
        restore_button = CreateRestoreGlobalButton(setting.UsingGlobal(), this);

        touch = [this]() {
            LOG_DEBUG(Frontend, "Setting custom setting for {}", setting.GetLabel());
            restore_button->setEnabled(true);
            restore_button->setVisible(true);
        };
    }

    if (require_checkbox) {
        QWidget* lhs =
            CreateCheckBox(other_setting, label, checkbox_serializer, checkbox_restore_func, touch);
        layout->addWidget(lhs);
    } else if (setting.TypeId() != typeid(bool)) {
        QLabel* qt_label = CreateLabel(label);
        layout->addWidget(qt_label);
    }

    if (setting.TypeId() == typeid(bool)) {
        data_component = CreateCheckBox(&setting, label, serializer, restore_func, touch);
    } else if (setting.IsEnum()) {
        data_component = CreateCombobox(serializer, restore_func, touch);
    } else if (type == typeid(u32) || type == typeid(int) || type == typeid(u16) ||
               type == typeid(s64) || type == typeid(u8)) {
        switch (request) {
        case RequestType::Slider:
        case RequestType::ReverseSlider:
            data_component = CreateSlider(request == RequestType::ReverseSlider, multiplier, string,
                                          serializer, restore_func, touch);
            break;
        case RequestType::Default:
        case RequestType::LineEdit:
            data_component = CreateLineEdit(serializer, restore_func, touch);
            break;
        case RequestType::DateTimeEdit:
            data_component = CreateDateTimeEdit(other_setting->ToString() != "true", true,
                                                serializer, restore_func, touch);
            break;
        case RequestType::SpinBox:
            data_component = CreateSpinBox(string, serializer, restore_func, touch);
            break;
        case RequestType::HexEdit:
            data_component = CreateHexEdit(serializer, restore_func, touch);
            break;
        case RequestType::ComboBox:
            data_component = CreateCombobox(serializer, restore_func, touch);
            break;
        default:
            UNIMPLEMENTED();
        }
    } else if (type == typeid(std::string)) {
        switch (request) {
        case RequestType::Default:
        case RequestType::LineEdit:
            data_component = CreateLineEdit(serializer, restore_func, touch);
            break;
        case RequestType::ComboBox:
            data_component = CreateCombobox(serializer, restore_func, touch);
            break;
        default:
            UNIMPLEMENTED();
        }
    }

    if (data_component == nullptr) {
        LOG_ERROR(Frontend, "Failed to create widget for {}", setting.GetLabel());
        created = false;
        return;
    }

    layout->addWidget(data_component);

    if (!managed) {
        return;
    }

    if (Settings::IsConfiguringGlobal()) {
        load_func = [this, serializer, checkbox_serializer, require_checkbox, other_setting]() {
            if (require_checkbox) {
                other_setting->LoadString(checkbox_serializer());
            }
            setting.LoadString(serializer());
        };
    } else {
        layout->addWidget(restore_button);

        QObject::connect(restore_button, &QAbstractButton::clicked,
                         [this, restore_func, checkbox_restore_func](bool) {
                             restore_button->setEnabled(false);
                             restore_button->setVisible(false);

                             checkbox_restore_func();
                             restore_func();
                         });

        load_func = [this, serializer, require_checkbox, checkbox_serializer, other_setting]() {
            bool using_global = !restore_button->isEnabled();
            setting.SetGlobal(using_global);
            if (!using_global) {
                setting.LoadString(serializer());
            }
            if (require_checkbox) {
                other_setting->SetGlobal(using_global);
                if (!using_global) {
                    other_setting->LoadString(checkbox_serializer());
                }
            }
        };
    }
}

bool Widget::Valid() const {
    return created;
}

Widget::~Widget() = default;

Widget::Widget(Settings::BasicSetting* setting_, const TranslationMap& translations_,
               const ComboboxTranslationMap& combobox_translations_, QWidget* parent_,
               bool runtime_lock_, std::forward_list<std::function<void(bool)>>& apply_funcs_,
               RequestType request, bool managed, float multiplier,
               Settings::BasicSetting* other_setting, const QString& string)
    : QWidget(parent_), parent{parent_}, translations{translations_},
      combobox_enumerations{combobox_translations_}, setting{*setting_}, apply_funcs{apply_funcs_},
      runtime_lock{runtime_lock_} {
    if (!Settings::IsConfiguringGlobal() && !setting.Switchable()) {
        LOG_DEBUG(Frontend, "\"{}\" is not switchable, skipping...", setting.GetLabel());
        return;
    }

    const int id = setting.Id();

    const auto [label, tooltip] = [&]() {
        const auto& setting_label = setting.GetLabel();
        if (translations.contains(id)) {
            return std::pair{translations.at(id).first, translations.at(id).second};
        }
        LOG_WARNING(Frontend, "Translation table lacks entry for \"{}\"", setting_label);
        return std::pair{QString::fromStdString(setting_label), QStringLiteral("")};
    }();

    if (label == QStringLiteral("")) {
        LOG_DEBUG(Frontend, "Translation table has emtpy entry for \"{}\", skipping...",
                  setting.GetLabel());
        return;
    }

    std::function<void()> load_func = []() {};

    SetupComponent(label, load_func, managed, request, multiplier, other_setting, string);

    if (!created) {
        LOG_WARNING(Frontend, "No widget was created for \"{}\"", setting.GetLabel());
        return;
    }

    apply_funcs.push_front([load_func, setting_](bool powered_on) {
        if (setting_->RuntimeModfiable() || !powered_on) {
            load_func();
        }
    });

    bool enable = runtime_lock || setting.RuntimeModfiable();
    if (setting.Switchable() && Settings::IsConfiguringGlobal() && !runtime_lock) {
        enable &= setting.UsingGlobal();
    }
    this->setEnabled(enable);

    this->setVisible(Settings::IsConfiguringGlobal() || setting.Switchable());

    this->setToolTip(tooltip);
}
} // namespace ConfigurationShared
