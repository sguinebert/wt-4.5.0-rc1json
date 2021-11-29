// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WT_DBO_DBACTION_H_
#define WT_DBO_DBACTION_H_

#include <set>
#include <string>
#include <sstream>
#include <type_traits>
#include <vector>

#include <Wt/Dbo/ptr.h>
#include <Wt/Dbo/collection.h>
#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/SqlStatement.h>
#include <Wt/Dbo/Session.h>
#include "Wt/WString.h"
#include "Wt/WAny.h"

namespace Wt {
  namespace Dbo {

template <class C, class Enable = void>
struct persist
{
  template <class A> static void apply(C& obj, A& action);
};

template <class A>
struct action_sets_value : std::false_type { };

class SqlStatement;

class WTDBO_API InitSchema
{
public:
  InitSchema(Session& session, Impl::MappingInfo& mapping);

  template<class C> void visit(C& obj);

  void actMapping(Impl::MappingInfo *mapping);
  template<typename V> void actId(V& value, const std::string& name, int size, int flags = 0);
  template<class C> void actId(ptr<C>& value, const std::string& name, int size,
			       int fkConstraints);
  template<typename V> void act(const FieldRef<V>& field);
  template<class C> void actPtr(const PtrRef<C>& field);
  template<class C> void actWeakPtr(const WeakPtrRef<C>& field);
  template<class C> void actCollection(const CollectionRef<C>& field);

  bool getsValue() const;
  bool setsValue() const;
  bool isSchema() const;
  virtual bool updateids() const { return false; }
  virtual std::vector<cpp17::any> insertedNaturalids() const { return std::vector<cpp17::any>(); }

  Session *session() { return &session_; }

private:
  Session& session_;
  Impl::MappingInfo& mapping_;

  bool idField_;
  bool nomutation_;
  std::string foreignKeyTable_, foreignKeyName_;
  int fkConstraints_, fkFlags_;
};

class WTDBO_API DropSchema
{
public:
  DropSchema(Session& session, 
	     Impl::MappingInfo& mapping,
	     std::set<std::string>& tablesCreated);

  template<class C> void visit(C& obj);

  void actMapping(Impl::MappingInfo *mapping);
  template<typename V> void actId(V& value, const std::string& name, int size, int flags = 0);
  template<class C> void actId(ptr<C>& value, const std::string& name, int size,
			       int fkConstraints);
  template<typename V> void act(const FieldRef<V>& field);
  template<class C> void actPtr(const PtrRef<C>& field);
  template<class C> void actWeakPtr(const WeakPtrRef<C>& field);
  template<class C> void actCollection(const CollectionRef<C>& field);

  bool getsValue() const;
  bool setsValue() const;
  bool isSchema() const;
  virtual bool updateids() const { return false; }
  virtual std::vector<cpp17::any> insertedNaturalids() const { return std::vector<cpp17::any>(); }

  Session *session() { return &session_; }

private:
  Session& session_;
  Impl::MappingInfo& mapping_;
  std::set<std::string>& tablesDropped_;

  void drop(const std::string& tableName);
};

class WTDBO_API DboAction
{
public:
  DboAction(Session *session);
  DboAction(MetaDboBase& dbo, Impl::MappingInfo& mapping);

  void actMapping(Impl::MappingInfo *mapping);
  template<class C> void actWeakPtr(const WeakPtrRef<C>& field);
  template<class C> void actCollection(const CollectionRef<C>& field);

  bool getsValue() const;
  bool setsValue() const;
  bool isSchema() const;
  virtual bool updateids() const { return false; }
  virtual std::vector<cpp17::any> insertedNaturalids() const { return std::vector<cpp17::any>(); }

  Session *session() { return dbo_ ? dbo_->session() : session_; }

protected:
  MetaDboBase& dbo() { return *dbo_; }
  Impl::MappingInfo& mapping() { return *mapping_; }
  int setStatementIdx() const { return setStatementIdx_; }
  
private:
  Session *session_;
  MetaDboBase *dbo_;
  Impl::MappingInfo *mapping_;

  int setStatementIdx_, setIdx_;
};

template <class C>
class UpdateBaseAction : public DboAction
{
public:
  UpdateBaseAction(MetaDbo<C>& dbo, Session::Mapping<C>& mapping);

  template<typename V> void act(const FieldRef<V>& field);
  template<class D> void actPtr(const PtrRef<D>& field);
  template<class D> void actWeakPtr(const WeakPtrRef<D>& field);
  template<class D> void actCollection(const CollectionRef<D>& field);
  template<typename V> void actId(V& value, const std::string& name, int size, int flags = 0);
  template<class D> void actId(ptr<D>& value, const std::string& name, int size, int fkConstraints);

  void visit(C& obj);
};

template <class C>
struct action_sets_value<UpdateBaseAction<C>> : std::true_type { };

class WTDBO_API LoadBaseAction : public DboAction
{
public:
  LoadBaseAction(MetaDboBase& dbo, Impl::MappingInfo& mapping,
		 SqlStatement *statement, int& column);

  template<typename V> void act(const FieldRef<V>& field);
  template<class D> void actPtr(const PtrRef<D>& field);

  bool setsValue() const;

protected:
  SqlStatement *statement_;
  int& column_;

  void start();
};

template <>
struct action_sets_value<LoadBaseAction> : std::true_type { };

template <class C>
class LoadDbAction : public LoadBaseAction
{
public:
  LoadDbAction(MetaDbo<C>& dbo, Session::Mapping<C>& mapping,
	       SqlStatement *statement, int& column);

  void visit(C& obj);

  template<typename V> void actId(V& value, const std::string& name, int size, int flags = 0);
  template<class D> void actId(ptr<D>& value, const std::string& name, int size,
			       int fkConstraints);

private:
  MetaDbo<C>& dbo_;
};

template <class C>
struct action_sets_value<LoadDbAction<C> > : std::true_type { };

class WTDBO_API SaveBaseAction : public DboAction
{
public:
  /*
   * Provide a statement and column if you want to abuse this action
   * to simply bind values to a statement using field().
   */
  SaveBaseAction(Session *session, SqlStatement *statement, int column);
  
  SaveBaseAction(MetaDboBase& dbo, Impl::MappingInfo& mapping,
		 SqlStatement *statement = nullptr, int column = 0);

  template<typename V> void act(const FieldRef<V>& field);
  template<class C> void actPtr(const PtrRef<C>& field);
  template<class C> void actWeakPtr(const WeakPtrRef<C>& field);
  template<class C> void actCollection(const CollectionRef<C>& field);
  template<typename V> void actId(V& value, const std::string& name, int size, int flags = 0);
  template<class D> void actId(ptr<D>& value, const std::string& name, int size,
			       int fkConstraints);

  template<class C> void visitAuxIds(C& obj);

  bool getsValue() const;

  int column() const { return column_; }

  void setNomutation(bool nomutation) { nomutation_ = nomutation;}
  void setIdOnly(bool idonly = true) { IdOnly_ = idonly;}
  virtual bool updateids() const override { return updateIds_; }
  virtual std::vector<cpp17::any> insertedNaturalids() const { return listids_; }

protected:
  SqlStatement *statement_;
  bool isInsert_;
  int column_;
  int naturalid_ = 0;
  bool bindNull_;
  bool auxIdOnly_;
  bool IdOnly_;
  bool updateIds_;
  bool nomutation_ = false;
  std::vector<cpp17::any> listids_;

  enum { Dependencies, Self, Sets } pass_;
  bool needSetsPass_;

  void startDependencyPass();
  void startSelfPass();
  void startSetsPass();

  void exec();
};

template <class C>
class SaveDbAction : public SaveBaseAction
{
public:
  SaveDbAction(MetaDbo<C>& dbo, Session::Mapping<C>& mapping);

  void visit(C& obj);

  template<typename V> void actId(V& value, const std::string& name, int size, int flags = 0);
  template<class D> void actId(ptr<D>& value, const std::string& name, int size,
			       int fkConstraints);

private:
  MetaDbo<C>& dbo_;
};

class WTDBO_API TransactionDoneAction : public DboAction
{
public:
  TransactionDoneAction(MetaDboBase& dbo, Session& session,
			Impl::MappingInfo& mapping, bool success);

  template<class C> void visit(C& obj);

  template<typename V> void actId(V& value, const std::string& name, int size, int flags = 0);
  template<class C> void actId(ptr<C>& value, const std::string& name, int size,
			       int fkConstraints);
  template<typename V> void act(const FieldRef<V>& field);
  template<class C> void actPtr(const PtrRef<C>& field);
  template<class C> void actWeakPtr(const WeakPtrRef<C>& field);
  template<class C> void actCollection(const CollectionRef<C>& field);

  bool getsValue() const;

  Session *session() { return &session_; }

private:
  Session& session_;
  bool success_;
};

class WTDBO_API SessionAddAction : public DboAction
{
public:
  SessionAddAction(MetaDboBase& dbo, Impl::MappingInfo& mapping);

  template<class C> void visit(C& obj);

  template<typename V> void actId(V& value, const std::string& name, int size, int flags = 0);
  template<class C> void actId(ptr<C>& value, const std::string& name, int size,
			       int fkConstraints);
  template<typename V> void act(const FieldRef<V>& field);
  template<class C> void actPtr(const PtrRef<C>& field);
  template<class C> void actWeakPtr(const WeakPtrRef<C>& field);
  template<class C> void actCollection(const CollectionRef<C>& field);

  bool getsValue() const;
  bool setsValue() const;
  bool isSchema() const;
};

class WTDBO_API SetReciproceAction
{
public:
  SetReciproceAction(Session *session,
		     const std::string& joinName, MetaDboBase *value);

  template<class C> void visit(C& obj);

  void actMapping(Impl::MappingInfo *mapping);
  template<typename V> void actId(V& value, const std::string& name, int size, int flags = 0);
  template<class C> void actId(ptr<C>& value, const std::string& name, int size,
			       int fkConstraints);
  template<typename V> void act(const FieldRef<V>& field);
  template<class C> void actPtr(const PtrRef<C>& field);
  template<class C> void actWeakPtr(const WeakPtrRef<C>& field);
  template<class C> void actCollection(const CollectionRef<C>& field);

  bool getsValue() const;
  bool setsValue() const;
  bool isSchema() const;

  Session *session() { return session_; }

private:
  Session *session_;
  const std::string& joinName_;
  MetaDboBase *value_;
};

template <>
struct action_sets_value<SetReciproceAction> : std::true_type { };

class WTDBO_API ToAnysAction
{
public:
  ToAnysAction(std::vector<cpp17::any>& result);

  template<class C> void visit(const ptr<C>& obj);

  void actMapping(Impl::MappingInfo *mapping);
  template<typename V> void actId(V& value, const std::string& name, int size, int flags = 0);
  template<class C> void actId(ptr<C>& value, const std::string& name, int size,
			       int fkConstraints);
  template<typename V> void act(const FieldRef<V>& field);
  template<class C> void actPtr(const PtrRef<C>& field);
  template<class C> void actWeakPtr(const WeakPtrRef<C>& field);
  template<class C> void actCollection(const CollectionRef<C>& field);

  bool getsValue() const;
  bool setsValue() const;
  bool isSchema() const;
  
    virtual bool updateids() const { return false; }
  virtual std::vector<cpp17::any> insertedNaturalids() const { return std::vector<cpp17::any>(); }

  Session *session() { return session_; }

private:
  Session *session_;
  std::vector<cpp17::any>& result_;
  bool allEmpty_;
};

class WTDBO_API FromAnyAction
{
public:
  FromAnyAction(int& index, const cpp17::any& value);

  template<class C> void visit(const ptr<C>& obj);

  void actMapping(Impl::MappingInfo *mapping);
  template<typename V> void actId(V& value, const std::string& name, int size, int flags = 0);
  template<class C> void actId(ptr<C>& value, const std::string& name, int size,
			       int fkConstraints);
  template<typename V> void act(const FieldRef<V>& field);
  template<class C> void actPtr(const PtrRef<C>& field);
  template<class C> void actWeakPtr(const WeakPtrRef<C>& field);
  template<class C> void actCollection(const CollectionRef<C>& field);

  bool getsValue() const;
  bool setsValue() const;
  bool isSchema() const;
  
    virtual bool updateids() const { return false; }
  virtual std::vector<cpp17::any> insertedNaturalids() const { return std::vector<cpp17::any>(); }

  Session *session() { return session_; }

private:
  Session *session_;
  int& index_;
  const cpp17::any& value_;
};

template <>
struct action_sets_value<FromAnyAction> : std::true_type { };

template<typename T>
struct Converter
{
	static cpp17::any get(const char* cstr)
	{
	  return cpp17::any();
	}
};

template<>
struct Converter<std::string>
{
	static cpp17::any get(const char* cstr)
	{
    	  cpp17::any val = std::string(cstr);
    	  return val;
	}
};


template<>
struct Converter<int>
{
	static cpp17::any get(const char* cstr)
	{
    	  cpp17::any val = std::stoi(cstr);
    	  return val;
	}
};
template<>
struct Converter<long long>
{
	static cpp17::any get(const char* cstr)
	{
    	  cpp17::any val = std::stoll(cstr);
    	  return val;
	}
};

template<typename V>
void SaveBaseAction::act(const FieldRef<V>& field)
{
  if(updateIds_){
    if((field.flags() & FieldRef<V>::Default) && naturalid_ < listids_.size()){

        //cpp17::any an = convertAnyToAny(listids_[naturalid_], *field.type());
    	auto cstr = cpp17::any_cast<char*>(listids_[naturalid_]);
	auto an = Converter<V>::get(cstr);
    	auto val = cpp17::any_cast<V>(an);
    	field.setValue(val);

    	naturalid_++;
    }
  return;
  }

  if ((auxIdOnly_ && !(field.flags() & FieldRef<V>::AuxId)) 
       || (nomutation_ && !IdOnly_)
       || (!isInsert_ && !IdOnly_ && (field.flags() & FieldRef<V>::NoMutation)) 
       || (isInsert_ && (field.flags() & FieldRef<V>::Default)) )
    return;


  if (pass_ == Self) {
    if (bindNull_)
      statement_->bindNull(column_++);
    else
      field.bindValue(statement_, column_++);
  }
}

  }
}

#endif // WT_DBO_DBACTION_H_
