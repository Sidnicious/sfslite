

// -*-c++-*-
/* $Id: tame_event.h 2678 2007-04-03 19:27:46Z max $ */

#ifndef _LIBTAME_TAME_EVENT_OPT_H_
#define _LIBTAME_TAME_EVENT_OPT_H_

#include "tame_event.h"
#include "tame_event_ag.h"
#include "tame_closure.h"
#include "tame_recycle.h"

//
// An experimental optimization for events are void
//
template<class T>
class green_event_t : public _event<T>
{
public:
  green_event_t (recycle_bin_t<green_event_t<T> > *rb,
		 const _tame_slot_set<T> &ss, closure_ptr_t c, 
		 const char *loc) 
    : _event<T> (ss, loc),
      _rb (rb),
      _closure (c)
  {}

  void reinit (const _tame_slot_set<T> &ss, closure_ptr_t c, const char *loc) 
  {
    _event_cancel_base::reinit (loc);
    slot_set_reassign (ss);
    _closure = c;
  }

  void clear_action ()
  {
    closure_ptr_t p = _closure;
    _closure = NULL;
    if (p) {
      p->remove (this);
    }
    p = NULL;
  }

  bool perform_action (_event_cancel_base *e, const char *loc, bool reuse)
  {
    bool ret = false;
    if (!_closure) {
      tame_error (loc, "event reused after deallocation");
    } else {
      if (_closure->block_dec_count (loc)) {
	_closure->v_reenter ();
      }
      clear_action ();
      ret = true;
    }
    return ret;
  }


  void finalize () { 
    clear_action ();
    _rb->recycle (this);
  }


  ~green_event_t () {}

  list_entry<green_event_t<T> > _lnk;

private:
  recycle_bin_t<green_event_t<T> > *_rb;
  closure_ptr_t _closure;
};

namespace green_event {

  recycle_bin_t<green_event_t<void> > *vrb ();

  template<class T>
  typename event<T>::ref
  alloc (recycle_bin_t<green_event_t<T> > *rb,
	 const _tame_slot_set<T> &ss,
	 ptr<closure_t> c, const char *loc)
  {
    ptr<green_event_t<T> > ret = rb->get ();
    if (ret) {
      ret->reinit (ss, c, loc);
      g_stats->evv_rec_hit ();
    } else {
      ret = New refcounted<green_event_t<T> > (rb, ss, c, loc);
      g_stats->evv_rec_miss ();
    }
    c->block_inc_count ();
    c->add (ret);
    return ret;
  }
}

template<class C>
typename event<>::ref
_mkevent (const closure_wrapper<C> &c, const char *loc)
{
  return green_event::alloc (green_event::vrb (), 
			     _tame_slot_set<> (), 
			     c.closure (), loc);
}

#define RECYCLE_EVENT_H(Typ,Sz,Nm)                                   \
namespace green_event {                                              \
  extern recycle_bin_t<green_event_t<Typ> > * _rb_##Nm;              \
}                                                                    \
template<class C>                                                    \
typename event<Typ>::ref                                             \
_mkevent (const closure_wrapper<C> &c, const char *loc, Typ &t)      \
{                                                                    \
  if (!green_event::_rb_##Nm)                                        \
    green_event::_rb_##Nm =                                          \
      New recycle_bin_t<green_event_t<Typ> > (Sz);                   \
  return green_event::alloc (green_event::_rb_##Nm,                  \
			     _tame_slot_set<Typ> (&t),               \
                             c.closure (), loc);                     \
}

#define RECYCLE_EVENT_C(Type,Nm)                                     \
namespace green_event {                                              \
   recycle_bin_t<green_event_t<Type> > *_rb_##Nm;                    \
}

RECYCLE_EVENT_H(bool, 0x10000, bool)
RECYCLE_EVENT_H(int, 0x10000, int)

#endif /* _LIBTAME_TAME_EVENT_OPT_H_ */
