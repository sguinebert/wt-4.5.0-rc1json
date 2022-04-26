#include "WTuiCalendar.h"

#include <Wt/WPushButton.h>
#include <Wt/WDateTime.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WVBoxLayout.h>
#include <Wt/WCheckBox.h>
#include <Wt/Json/Parser.h>

namespace Wt {

static const std::string datebase {
    "{"
        "id: '{}',"
        "calendarId: '{}',"
        "title: '{}',"
        "category: 'time',"
        "dueDateClass: '',"
        "start: '{}',"
        "end: '{}'"
    "}"
};

WTuiCalendar::WTuiCalendar() :
    tuidateChange_(this, "dateChange"),
    j_newSchedule_(this, "newSchedule"),
    j_updateSchedule_(this, "updateSchedule"),
    j_deleteSchedule_(this, "deleteSchedule")
{
    tuidateChange_.connect([=] (std::string datestr) {
        auto date = WDate::fromString(datestr, "yyyy-MM-dd");
        dateChange_.emit(date);
    });
    j_newSchedule_.connect(this, &WTuiCalendar::newSchedule);
    j_updateSchedule_.connect(this, &WTuiCalendar::updateSchedule);

    auto hlayout = this->setLayout(std::make_unique<WHBoxLayout>());
    auto leftcontainer = hlayout->addWidget(std::make_unique<WContainerWidget>());
    leftcontainer->resize(200, WLength::Auto);
    auto viewAllcontainer = leftcontainer->addNew<WContainerWidget>();
    auto calendarList = leftcontainer->addNew<WContainerWidget>();
    auto vlayout = hlayout->addLayout(std::make_unique<WVBoxLayout>(), 1);
    auto navbar = vlayout->addWidget(std::make_unique<WContainerWidget>());
    auto calendar = vlayout->addWidget(std::make_unique<WContainerWidget>());

    viewAllcontainer->setStyleClass("lnb-calendars-item");
    auto viewAll = viewAllcontainer->addNew<WCheckBox>("View all");
    viewAll->setChecked(true);
    //viewAll->decorationStyle().setTextDecoration
    viewAll->setStyleClass("tui-full-calendar-checkbox-square");
    viewAll->setAttributeValue("value", "all");

    auto today = navbar->addNew<WPushButton>("Today");
    today->setStyleClass("btn btn-default btn-sm move-today");
    today->setAttributeValue("data-action", "move-today");

    auto prev = navbar->addNew<WPushButton>("&#8249;", TextFormat::XHTML);
    //prev->setIcon("");
    prev->setStyleClass("btn btn-default btn-sm move-day");
    prev->setAttributeValue("data-action", "move-prev");

    auto next = navbar->addNew<WPushButton>("&#8250;", TextFormat::XHTML);
    //prev->setIcon("");
    next->setStyleClass("btn btn-default btn-sm move-day");
    next->setAttributeValue("data-action", "move-next");

    auto range = navbar->addNew<WText>("");
    range->setStyleClass("render-range");


    auto app = WApplication::instance();

    app->require("https://uicdn.toast.com/tui.code-snippet/v1.5.2/tui-code-snippet.min.js");
    //    app_->require("https://uicdn.toast.com/tui.time-picker/v2.0.3/tui-time-picker.min.js");
    //    app_->require("https://uicdn.toast.com/tui.date-picker/v4.0.3/tui-date-picker.min.js");
    app->useStyleSheet("css/tui-calendar.min.css");
    app->require("js/tui-calendar.js");



    doJavaScript("var date = new Date();"
                 "var offset = date.getTimezoneOffset();"
                 "var calendar = new tui.Calendar("+ calendar->jsRef() +", {"
                    "startDayOfWeek:0,"
                     "calendars: ["
                     "{"
                         "id: '1',"
                         "name: 'My Calendar',"
                         "color: '#ffffff',"
                         "bgColor: '#9e5fff',"
                         "dragBgColor: '#9e5fff',"
                         "borderColor: '#9e5fff'"
                     "},"
                     "{"
                         "id: '2',"
                         "name: 'Company',"
                         "color: '#00a9ff',"
                         "bgColor: '#00a9ff',"
                         "dragBgColor: '#00a9ff',"
                         "borderColor: '#00a9ff'"
                     "},"
                    "],"
//                    "schedules:[{}],"
                    //"daynames:[],"
                    //"workweek:false,"
                    "taskView: false,"  // e.g. true, false, or ['task', 'milestone']
                    "scheduleView: ['time']," // e.g. true, false, or ['allday', 'time']
                    "defaultView: 'week'," // weekly view option

                    "useCreationPopup: false," // whether use default creation popup or not
                    "useDetailPopup: false," // whether use default detail popup or not
                    "timezone: {"
                        "offsetCalculator: function(timezoneName, timestamp){"
                          // matches 'getTimezoneOffset()' of Date API
                          // e.g. +09:00 => -540, -04:00 => 240
                          "var date = new Date();"
                          "var offset = date.getTimezoneOffset();"

                          "return offset;"
                        "}"
                    "},"
                    "template: {"
                        "timegridDisplayPrimaryTime: function(time) {"
                            "var h = time.hour < 10 ? '0'+time.hour : time.hour;"
                            "var m = time.minutes < 10 ? '0'+time.minutes : time.minutes;"

                            "return h+':'+m;"
                        "},"
                        "timegridDisplayTime: function(time) {"
                            "return time.hour+':'+time.minutes;"
                        "},"
                    "},"
                 "});"
                 "calendar.on('clickSchedule', function(event){"
                     "var schedule = event.schedule;"
                     "console.log('click', schedule);"

                 "});"
                 "calendar.on('beforeCreateSchedule', function(event) {"
                     "var startTime = event.start;"
                     "var endTime = event.end;"
                     "var isAllDay = event.isAllDay;"
                     "var guide = event.guide;"
                     "var triggerEventName = event.triggerEventName;"
                     "var json = {startTime:startTime,"
                                  "endTime:endTime,"
                                  "isAllDay:isAllDay,"
                                  //"guide:guide,"
                                  "triggerEventName:triggerEventName,"
                     "};"
                     "console.log('create test', JSON.stringify(json));"
                     "Wt.emit(" + this->jsRef() + ", 'newSchedule', JSON.stringify(json));"
                 "});"
                 "calendar.on('beforeDeleteSchedule', function(event) {"
                       "var schedule = event.schedule;"
                       "console.log('The schedule is removed.', schedule);"
                 "});"

                 "calendar.on('beforeUpdateSchedule', function(event) {"
                    "var schedule = event.schedule;"
                    "var changes = event.changes;"
                    "calendar.updateSchedule(schedule.id, schedule.calendarId, changes);"
                    "Wt.emit(" + this->jsRef() + ", 'updateSchedule', JSON.stringify(schedule));"
                "});");

    doJavaScript(today->jsRef() + ".addEventListener('click', e => {"
                     "calendar.today();"
                     "var date = calendar.getDateRangeStart().toDate();"
                     "Wt.emit("+ jsRef() +", 'dateChange', date.toISOString().slice(0,10));"
                     "setRenderRangeText();"
                 "});"
                + prev->jsRef() + ".addEventListener('click', e => {"
                       "calendar.prev();"
                       "var date = calendar.getDateRangeStart().toDate();"
                       "Wt.emit("+ jsRef() +", 'dateChange', date.toISOString().slice(0,10));"
                       "setRenderRangeText();"
                "});"
               + next->jsRef() + ".addEventListener('click', e => {"
                      "calendar.next();"
                      "var date = calendar.getDateRangeStart().toDate();"
                      "Wt.emit("+ jsRef() +", 'dateChange', date.toISOString().slice(0,10));"
                      "setRenderRangeText();"
                "});"

                 "function setRenderRangeText() {"
                     "var renderRange = "+ range->jsRef() +";"
                     "var options = calendar.getOptions();"
                     "var viewName = calendar.getViewName();"

                     "var html = [];"
                     "if (viewName === 'day') {"
                         "html.push(currentCalendarDate('YYYY.MM.DD'));"
                     "} else if (viewName === 'month' && (!options.month.visibleWeeksCount || options.month.visibleWeeksCount > 4)) {"
                         "html.push(currentCalendarDate('YYYY.MM'));"
                     "} else {"
//                         "html.push(moment(calendar.getDateRangeStart().getTime()).format('YYYY.MM.DD'));"
//                         "html.push(' ~ ');"
//                         "html.push(moment(calendar.getDateRangeEnd().getTime()).format(' MM.DD'));"
                     "}"
                     "renderRange.innerHTML = html.join('');"
                 "}");

    doJavaScript("var calendarList = "+ calendarList->jsRef() +";"
                    "console.log('cc', calendarList, calendar);"
                    "var html = [];"
                    "calendar._options.calendars.forEach(function(ca) {"
                        "html.push('<div class=\"lnb-calendars-item\"><label>' +"
                            "'<input type=\"checkbox\" class=\"tui-full-calendar-checkbox-round\" value=\"' + ca.id + '\" checked>' +"
                            "'<span style=\"border-color: ' + ca.borderColor + '; background-color: ' + ca.borderColor + ';\"></span>' +"
                            "'<span>' + ca.name + '</span>' +"
                            "'</label></div>'"
                        ");"
                    "});"
                    "calendarList.innerHTML = html.join(' ');"
                 "calendarList.addEventListener('click', onChangeCalendars);"
                 + viewAllcontainer->jsRef() + ".addEventListener('click', onChangeCalendars);"
                 "function findCalendar(id) {"
                     "var found;"
                     "calendar._options.calendars.forEach(function(calendar) {"
                         "if (calendar.id === id) {"
                             "found = calendar;"
                         "}"
                     "});"

                     "return found || calendar._options.calendars[0];"
                 "};"
                 "function onChangeCalendars(e) {"
                     "var calendarId = e.target.value;"
                     "var checked = e.target.checked;"
                     "var viewAll = "+ viewAllcontainer->jsRef() +".querySelector('input');"
                     "var calendarElements = "+ calendarList->jsRef() +".querySelectorAll('input');"//Array.prototype.slice.call(document.querySelectorAll('#calendarList input'));"

                      "if(!calendarId)"
                          "return;"
                     "var allCheckedCalendars = true;"

                     "if (calendarId === 'all') {"
                         "allCheckedCalendars = checked;"
                         "calendarElements.forEach(function(input) {"
                             "var span = input.parentNode;"
                             "input.checked = checked;"
                             "span.style.backgroundColor = checked ? span.style.borderColor : 'transparent';"
                         "});"

                         "calendar._options.calendars.forEach(function(cal) {"
                             "cal.checked = checked;"
                         "});"
                     "} else {"
                         "findCalendar(calendarId).checked = checked;"

                         "for(var i=0; i < calendarElements.length; i++){"
                              "if(!calendarElements[i].checked){"
                                  "allCheckedCalendars=false;"
                                  "break;"
                              "}"
                         "}"
                         "viewAll.checked = allCheckedCalendars;"
                     "}"

                     "refreshScheduleVisibility(calendarElements);"
                 "};"
                 "function refreshScheduleVisibility(calendarElements) {"
                     "calendar._options.calendars.forEach(function(cal) {"
                         "calendar.toggleSchedules(cal.id, !cal.checked, false);"
                     "});"

                     "calendar.render(true);"

                     "calendarElements.forEach(function(input) {"
                         "var span = input.nextElementSibling;"
                         "span.style.backgroundColor = input.checked ? span.style.borderColor : 'transparent';"
                     "});"
                 "};"
                 "");

    auto bdatetime = WDateTime::currentDateTime();
    auto edatetime = WDateTime::currentDateTime().addSecs(300);

    std::cout << bdatetime.toString("yyyy-MM-ddThh:mm:ss") << std::endl;


    pushSchedules_.setJavaScript("function(o, e, a1){ calendar.createSchedules(a1); }", 1);
    doJavaScript("calendar.createSchedules(["
                    "{"
                        "id: '1',"
                        "calendarId: '1',"
                        "title: 'my schedule',"
                        "category: 'time',"
                        "dueDateClass: '',"
                        "start: '2022-03-15T22:30:00',"
                        "end: '2022-03-15T23:30:00'"
                    "},"
                    "{"
                        "id: '2',"
                        "calendarId: '2',"
                        "title: 'second schedule',"
                        "category: 'time',"
                        "dueDateClass: '',"
                        "start: '" + bdatetime.toString("yyyy-MM-ddThh:mm:ss").toUTF8() + "',"
                        "end: '" + edatetime.toString("yyyy-MM-ddThh:mm:ss").toUTF8() + "'"
                    "}"
                 "]);"
                 "calendar.render();");

//    Wt::Json::Object obj {
//        {"id", "" },
//        {"calendarId", "" },
//        {"title", "" },
//        {"category", "time" },
//        {"dueDateClass", "" },
//        {"start", "" },
//        {"end", "" },
//        };

}

void WTuiCalendar::pushSchedules(const Json::Array &schedules)
{
    pushSchedules_.exec("''", "''", Wt::Json::serialize(schedules));
}

void WTuiCalendar::newSchedule(std::string schedule)
{
    //rdv.at("");
    //std::cout << schedule << std::endl;
    Wt::Json::Object obj;
    Wt::Json::parse(schedule, obj);
    newSchedule_.emit(obj);
}

void WTuiCalendar::updateSchedule(std::string schedule)
{
    //std::cout << schedule << std::endl;
    Wt::Json::Object obj;
    Wt::Json::parse(schedule, obj);
    updateSchedule_.emit(obj);
}

void WTuiCalendar::deleteSchedule(std::string schedule)
{
    Wt::Json::Object obj;
    Wt::Json::parse(schedule, obj);
    deleteSchedule_.emit(obj);
}

}
