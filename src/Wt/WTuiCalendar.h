#ifndef WTUICALENDAR_H
#define WTUICALENDAR_H

#include <Wt/WContainerWidget.h>
#include <Wt/WApplication.h>
#include <Wt/WDate.h>
#include <Wt/Json/json.hpp>

namespace Wt {

class WTuiCalendar : public WContainerWidget
{
public:
    WTuiCalendar();

    Signal<WDate>& dateChange() { return dateChange_; }
    Signal<Wt::Json::Object>& newSchedule() { return newSchedule_; }
    Signal<Wt::Json::Object>& updateSchedule() { return updateSchedule_; }
    Signal<Wt::Json::Object>& deleteSchedule() { return deleteSchedule_; }

    void pushSchedules(const Wt::Json::Array& schedules);
private:
    Signal<WDate> dateChange_;
    Signal<Wt::Json::Object> newSchedule_;
    Signal<Wt::Json::Object> updateSchedule_;
    Signal<Wt::Json::Object> deleteSchedule_;
    JSignal<std::string> tuidateChange_;
    JSignal<std::string> j_newSchedule_;
    JSignal<std::string> j_updateSchedule_;
    JSignal<std::string> j_deleteSchedule_;
    JSlot pushSchedules_;

private:
    void newSchedule(std::string schedule);
    void updateSchedule(std::string schedule);
    void deleteSchedule(std::string schedule);
};

}
#endif // WTUICALENDAR_H
