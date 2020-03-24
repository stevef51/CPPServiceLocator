/*
   Copyright 2020 Steve Fillingham

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef ServiceLocator_hpp
#define ServiceLocator_hpp

#include <string>
#include <map>
#include <list>
#include <set>
#include <typeindex>
#include <cxxabi.h>
#include <functional>
#include <memory>

#ifndef SERVICELOCATOR_SPTR
#define SERVICELOCATOR_SPTR
template <class T>
using sptr = std::shared_ptr<T>;

template <class T>
using const_sptr = std::shared_ptr<const T>;

template <class T>
using wptr = std::weak_ptr<T>;

template <class T>
using uptr = std::unique_ptr<T>;
#endif

class ServiceLocatorException {
private:
    std::string _message;

public:
    ServiceLocatorException(const std::string& message) : _message(message) {
    }

    const std::string& getMessage() const {
        return _message;
    }
};

class DuplicateBindingException : public ServiceLocatorException {
public:
    DuplicateBindingException(const std::string& message) : ServiceLocatorException(message) {
    }
};

class RecursiveResolveException : public ServiceLocatorException {
public:
    RecursiveResolveException(const std::string& message) : ServiceLocatorException(message) {
    }
};

class BindingIssueException : public ServiceLocatorException {
public:
    BindingIssueException(const std::string& message) : ServiceLocatorException(message) {
    }
};

class UnableToResolveException : public ServiceLocatorException {
public:
    UnableToResolveException(const std::string& message) : ServiceLocatorException(message) {
    }
};

class ServiceLocator {
public:
    friend class Context;
    
    class Context {
    private:
        // Only the root Context will run the AfterResolveList - this allows circular dependancies to
        // resolve by using afterResolve property injection
        Context* _root;
        std::list<std::function<void(sptr<Context>)>>* _fnAfterResolveList = nullptr;
        
        Context* _parent;
        wptr<ServiceLocator> _sl;
        std::type_index _interfaceType;
        mutable uptr<std::string> _interfaceTypeName;
        std::string _name;
        
        uptr<std::type_index> _concreteType;
        mutable uptr<std::string> _concreteTypeName;
        
        
        std::string getTypeName(const std::type_index& typeIndex) const {
            int status;
            auto s = __cxxabiv1::__cxa_demangle (typeIndex.name(), nullptr, nullptr, &status);
            std::string result;
            switch(status) {
                case 0:
                    result = std::string(s);
                    break;
                case 1:
                    result = "Memory failure";
                    break;
                case 2:
                    result = "Not a mangled name";
                    break;
                case 3:
                    result = "Invalid arguments";
                    break;
            }
            if (s) {
                delete s;
            }
            return result;
        }
        
        void checkRecursiveResolve(Context* resolveCtx, Context* compareCtx) {
            if (resolveCtx->_interfaceType == compareCtx->_interfaceType && resolveCtx->_name == compareCtx->_name) {
                throw RecursiveResolveException("Recursive resolve path = " + resolveCtx->getResolvePath());
            }
            if (compareCtx->_parent != nullptr) {
                checkRecursiveResolve(resolveCtx, compareCtx->_parent);
            }
        }

        void afterResolve() {
            if (this == _root) {
                if (_fnAfterResolveList != nullptr) {
                    for(auto fn : *_fnAfterResolveList) {
                        auto ctx = sptr<Context>(new Context(_sl));
                        fn(ctx);
                    }
                    delete _fnAfterResolveList;
                    _fnAfterResolveList = nullptr;
                }
            }
        }
        
    public:
        Context(Context* root, Context* parent, wptr<ServiceLocator> sl, const std::type_index interfaceType, const std::string& name) : _root(root), _parent(parent), _sl(sl), _interfaceType(interfaceType), _name(name) {
        }

        Context(Context* parent, const std::type_index interfaceType, const std::string& name) : Context(parent->_root, parent, parent->_sl, interfaceType, name) {
        }

        Context(wptr<ServiceLocator> sl, const std::type_index interfaceType, const std::string& name) : Context(this, nullptr, sl, interfaceType, name) {
        }

        Context(wptr<ServiceLocator> sl) : Context(this, nullptr, sl, std::type_index(typeid(void)), "") {
        }
        
        const std::string& getName() const {
            return _name;
        }
        
        const std::string& getInterfaceTypeName() const {
            if (_interfaceTypeName == nullptr) {
                _interfaceTypeName = uptr<std::string>(new std::string(getTypeName(_interfaceType)));
            }
            return *_interfaceTypeName;
        }
        
        const std::type_index& getInterfaceTypeIndex() const {
            return _interfaceType;
        }
        
        void setConcreteType(const std::type_index& concreteType) {
            if (_concreteType != nullptr) {
                throw BindingIssueException("Concrete type on Context already set");
            }
            _concreteType = uptr<std::type_index>(new std::type_index(concreteType));
        }
        
        const std::string& getConcreteTypeName() const {
            if (_concreteTypeName == nullptr) {
                _concreteTypeName = uptr<std::string>(new std::string(getTypeName(*_concreteType)));
            }
            return *_concreteTypeName;
        }
        
        const std::type_index& getConcreteTypeIndex() const {
            return *_concreteType;
        }

        Context* getParent() const {
            return _parent;
        }
        
        sptr<ServiceLocator> getServiceLocator() const {
            return _sl.lock();
        }
        
        // Resolve a named interface, throws if not able to resolve
        template <class IFace>
        sptr<IFace> resolve(const std::string& named) {
            auto ctx = sptr<Context>(new Context(this, std::type_index(typeid(IFace)), named));
            checkRecursiveResolve(ctx.get(), this);
            auto ptr = _sl.lock()->_resolve<IFace>(ctx);
            afterResolve();
            return ptr;
        }

        // Resolve an interface, throws if not able to resolve
        template <class IFace>
        sptr<IFace> resolve() {
            auto ctx = sptr<Context>(new Context(this, std::type_index(typeid(IFace)), ""));
            checkRecursiveResolve(ctx.get(), this);
            auto ptr = _sl.lock()->_resolve<IFace>(ctx);
            afterResolve();
            return ptr;
        }

        template <class IFace>
        void resolveAll(std::vector<sptr<IFace>>* all) {
            _sl.lock()->_visitAll<IFace>([this, all] (sptr<typename TypedServiceLocator<IFace>::shared_ptr_binding> binding) {
                auto ctx = sptr<Context>(new Context(this, std::type_index(typeid(IFace)), ""));
                checkRecursiveResolve(ctx.get(), this);
                all->push_back(binding->get(ctx));
            });
            afterResolve();
        }
        
        // Determine if a named interface can be resolved
        template <class IFace>
        bool canResolve(const std::string& named) {
            auto ctx = sptr<Context>(new Context(this, std::type_index(typeid(IFace)), named));
            return _sl.lock()->_canResolve<IFace>(ctx);
        }

        // Determine if an interface can be resolved
        template <class IFace>
        bool canResolve() {
            auto ctx = sptr<Context>(new Context(this, std::type_index(typeid(IFace)), ""));
            return _sl.lock()->_canResolve<IFace>(ctx);
        }

        // Try to resolve a named interface, returns nullptr on failure
        template <class IFace>
        sptr<IFace> tryResolve(const std::string& named) {
            auto ctx = sptr<Context>(new Context(this, std::type_index(typeid(IFace)), named));
            checkRecursiveResolve(ctx.get(), this);
            auto ptr = _sl.lock()->_tryResolve<IFace>(ctx);
            afterResolve();
            return ptr;
        }

        // Try to resolve an interface, returns nullptr on failure
        template <class IFace>
        sptr<IFace> tryResolve() {
            auto ctx = sptr<Context>(new Context(this, std::type_index(typeid(IFace)), ""));
            checkRecursiveResolve(ctx.get(), this);
            auto ptr = _sl.lock()->_tryResolve<IFace>(ctx);
            afterResolve();
            return ptr;
        }
        
        template <class IFace>
        std::function<sptr<IFace>(const std::string&)> provider() {
            // We lock the weak_ptr to our ServiceLocator, the lock returns a shared_ptr which will keep
            // it alive into the returned lambda via the capture of sl
            auto sl = _sl.lock();
            return [sl] (const std::string& name = "") {
                auto ctx = sptr<Context>(new Context(sl, std::type_index(typeid(IFace)), name));
                // Don't need to check for recursive resolve since this is a provider (root) call
                auto ptr = sl->_resolve<IFace>(ctx);
                // ctx is root Context, it can afterResolve
                ctx->afterResolve();
                return ptr;
            };
        }
        
        template <class IFace>
        std::function<sptr<IFace>(const std::string&)> tryProvider() {
            // We lock the weak_ptr to our ServiceLocator, the lock returns a shared_ptr which will keep
            // it alive into the returned lambda via the capture of sl
            auto sl = _sl.lock();
            return [sl] (const std::string& name = "") {
                auto ctx = sptr<Context>(new Context(sl, std::type_index(typeid(IFace)), name));
                // Don't need to check for recursive resolve since this is a tryProvider (root) call
                auto ptr = sl->_tryResolve<IFace>(ctx);
                // ctx is root Context, it can afterResolve
                ctx->afterResolve();
                return ptr;
            };
        }
        
        std::string getResolvePath() const {
            std::string path = "";
            // Note the root Parent has a <ServiceLocator> IFace which is not real, just cannot have no interface defined
            if (_parent != nullptr && _parent->_parent != nullptr) {
                path = _parent->getResolvePath() + " -> ";
            }
            
            path += "resolve<" + getInterfaceTypeName() + ">(" + _name + ")";

            if (_concreteType != nullptr) {
                path += ".to<" + getConcreteTypeName() + ">";
            }
            
            return path;
        }
        
        void afterResolve(std::function<void(sptr<Context>)> fnAfterResolve) {
            if (_root->_fnAfterResolveList == nullptr) {
                _root->_fnAfterResolveList = new std::list<std::function<void(sptr<Context>)>>();
            }
            _root->_fnAfterResolveList->push_back(fnAfterResolve);
        }
    };
    
private:
    class AnyServiceLocator {
    public:
        virtual ~AnyServiceLocator() {
        }
        
        class loose_binding {
        public:
            virtual ~loose_binding() {
            }
            
            virtual void eagerBind(sptr<Context> slc) = 0;
        };
    };
    
    template <class IFace>
    class TypedServiceLocator : public AnyServiceLocator {
    public:
        class shared_ptr_binding : public loose_binding {
        private:
            std::function<sptr<IFace>(sptr<Context>)> _fnGet;
            std::function<sptr<IFace>(sptr<Context>)> _fnCreate;
            
            // Note, this is used during binding only ..
            std::list<loose_binding*>* _eagerBindings;

        public:
            class eagerly_clause {
            private:
                shared_ptr_binding* _ibinding;
                
            public:
                eagerly_clause(shared_ptr_binding* ibinding) : _ibinding(ibinding) {
                }
                
                void eagerly() {
                    _ibinding->_eagerBindings->push_back(_ibinding);
                }
            };
            
            class as_clause {
            private:
                sptr<IFace> _singleton;
                shared_ptr_binding* _ibinding;
                
            public:
                as_clause(shared_ptr_binding* ibinding) : _ibinding(ibinding) {
                }
                
                eagerly_clause& asSingleton() {
                    // on 1st call we create the singleton ..
                    _ibinding->_fnGet = [this] (sptr<Context> slc) {
                        _singleton = _ibinding->_fnCreate(slc);
                        
                        auto singleton = _singleton;
                        
                        // rebind fnGet to return the new singleton
                        _ibinding->_fnGet = [this] (sptr<Context> slc) {
                            return _singleton;
                        };
                        
                        return singleton;
                    };
                    return _ibinding->_eagerly_clause;
                }

                void asTransient() {
                    _ibinding->_fnGet = _ibinding->_fnCreate;
                }
            };

            class to_clause {
            private:
                shared_ptr_binding* _ibinding;
                
            public:
                to_clause(shared_ptr_binding* ibinding) : _ibinding(ibinding) {
                }
                
                void toInstance(sptr<IFace> instance) {
                    // fnCreate is not needed, we always return 'instance'
                    _ibinding->_fnGet = [instance] (sptr<Context> slc) {
                        return instance;
                    };
                }

                void toInstance(IFace* instance) {
                    auto cached = sptr<IFace>(instance);
                    // fnCreate is not needed, we always return 'instance'
                    _ibinding->_fnGet = [cached] (sptr<Context> slc) {
                        return cached;
                    };
                }

                as_clause& toSelf() {
                    _ibinding->_fnGet = _ibinding->_fnCreate = [] (sptr<Context> slc) {
                        slc->setConcreteType(std::type_index(typeid(IFace)));
                        return sptr<IFace>(new IFace(slc));
                    };
                    return _ibinding->_as_clause;
                }
                
                as_clause& toSelfNoDependancy() {
                    _ibinding->_fnGet = _ibinding->_fnCreate = [] (sptr<Context> slc) {
                        slc->setConcreteType(std::type_index(typeid(IFace)));
                        return sptr<IFace>(new IFace());
                    };
                    return _ibinding->_as_clause;
                }
                
                template <class TImpl>
                as_clause& to() {
                    _ibinding->_fnGet = _ibinding->_fnCreate = [] (sptr<Context> slc) {
                        slc->setConcreteType(std::type_index(typeid(TImpl)));
                        return sptr<TImpl>(new TImpl(slc));
                    };
                    return _ibinding->_as_clause;
                }
                
                template <class TImpl>
                as_clause& toNoDependancy() {
                    _ibinding->_fnGet = _ibinding->_fnCreate = [] (sptr<Context> slc) {
                        slc->setConcreteType(std::type_index(typeid(TImpl)));
                        return sptr<TImpl>(new TImpl());
                    };
                    return _ibinding->_as_clause;
                }
                
                template <class TImpl>
                as_clause& to(std::function<sptr<TImpl>(sptr<Context>)> fnCreate) {
                    _ibinding->_fnGet = _ibinding->_fnCreate = [fnCreate] (sptr<Context> slc) {
                        slc->setConcreteType(std::type_index(typeid(TImpl)));
                        return fnCreate(slc);
                    };
                    return _ibinding->_as_clause;
                }

                // similar to above, except caller can return IFace* instead of sptr<IFace>
                template <class TImpl>
                as_clause& to(std::function<TImpl*(sptr<Context>)> fnCreate) {
                    _ibinding->_fnGet = _ibinding->_fnCreate = [fnCreate] (sptr<Context> slc) {
                        slc->setConcreteType(std::type_index(typeid(TImpl)));
                        // create sptr around the returned ptr
                        auto ptr = fnCreate(slc);
                        return sptr<TImpl>(ptr);
                    };
                    return _ibinding->_as_clause;
                }
                
                as_clause& alias(const std::string& name) {
                    _ibinding->_fnGet = _ibinding->_fnCreate = [name] (sptr<Context> slc) {
                        return slc->resolve<IFace>(name);
                    };
                    return _ibinding->_as_clause;
                }

                template <class IAlias>
                as_clause& alias() {
                    _ibinding->_fnGet = _ibinding->_fnCreate = [] (sptr<Context> slc) {
                        return slc->resolve<IAlias>(slc->getName());
                    };
                    return _ibinding->_as_clause;
                }
                
                template <class IAlias>
                as_clause& alias(const std::string& name) {
                    _ibinding->_fnGet = _ibinding->_fnCreate = [name] (sptr<Context> slc) {
                        return slc->resolve<IAlias>(name);
                    };
                    return _ibinding->_as_clause;
                }

            };
        
            to_clause _to_clause;
            as_clause _as_clause;
            eagerly_clause _eagerly_clause;
            
        public:
            shared_ptr_binding(std::list<loose_binding*>* eagerBindings)
                :
                _eagerBindings(eagerBindings),
                _to_clause(this),
                _as_clause(this),
                _eagerly_clause(this) {
            }
            
            virtual sptr<IFace> get(sptr<Context> slc) const {
                return _fnGet(slc);
            }
            
            void eagerBind(sptr<Context> slc) override {
                auto ctx = sptr<Context>(new Context(slc.get(), std::type_index(typeid(IFace)), ""));
                _fnGet(ctx);
            }
        };
        
        std::map<std::string, sptr<loose_binding>> _bindings;

    public:
        typename shared_ptr_binding::to_clause& bind(const std::string& name, std::list<loose_binding*>* eagerBindings) {
            if (canResolve(name)) {
                throw DuplicateBindingException(std::string("Duplicate binding for <") + typeid(IFace).name() + "> named " + name);
            }

            auto binding = sptr<shared_ptr_binding>(new shared_ptr_binding(eagerBindings));
            
            // (non const) IFace binding
            _bindings.insert(std::pair<std::string, sptr<loose_binding>>(name, binding));
            
            return binding->_to_clause;
        }

        bool canResolve(const std::string& name) {
            return _bindings.find(name) != _bindings.end();
        }

        sptr<IFace> tryResolve(const std::string& name, sptr<Context> slc) {
            auto binding = _bindings.find(name);
            if (binding == _bindings.end()) {
                return nullptr;
            }
            auto ibinding = std::dynamic_pointer_cast<shared_ptr_binding>(binding->second);
            
            return ibinding->get(slc);
        }
        
        void visitAll(std::function<void(sptr<TypedServiceLocator<IFace>::shared_ptr_binding>)> fnVisit) {
            for(auto binding : _bindings) {
                auto ibinding = std::dynamic_pointer_cast<shared_ptr_binding>(binding.second);
                fnVisit(ibinding);
            }
        }
    };
    
    // Named locator bindings (simple map from string to NamedServiceLocator)
    std::map<std::type_index, AnyServiceLocator*> _typed_locators;
    mutable std::list<AnyServiceLocator::loose_binding*> _eagerBindings;
    
    sptr<ServiceLocator> _parent;
    sptr<Context> _context;
    
    // We store a weak_ptr to ourselves so that we can create shared_ptr's from it when we enter() child
    // locators
    wptr<ServiceLocator> _this;
    
    template <class IFace>
    TypedServiceLocator<IFace>* getTypedServiceLocator(bool createIfRequired) {
        auto typeIndex = std::type_index(typeid(IFace));
        auto find = _typed_locators.find(typeIndex);
        if (find != _typed_locators.end()) {
            return dynamic_cast<TypedServiceLocator<IFace>*>(find->second);
        }
        
        if (!createIfRequired) {
            return nullptr;
        }
        
        auto nsl = new TypedServiceLocator<IFace>();
        _typed_locators.insert(std::pair<std::type_index, AnyServiceLocator*>(typeIndex, nsl));
        return nsl;
    }
    
    // Hide default constructor - client should call ::create which returns a shared_ptr version
    ServiceLocator() : ServiceLocator(nullptr) {
    }

    // Child locators keep a shared_ptr to their parent
    ServiceLocator(sptr<ServiceLocator> parent) : _parent(parent) {
    }
    
    // Resolve a named interface, throws if not able to resolve
    template <class IFace>
    sptr<IFace> _resolve(sptr<Context> slc) {
        auto& name = slc->getName();
        auto nsl = getTypedServiceLocator<IFace>(false);
        if (nsl == nullptr) {
            if (_parent == nullptr) {
                throw UnableToResolveException(std::string("Unable to resolve <") + slc->getInterfaceTypeName() + ">  resolve path = " + slc->getResolvePath());
            }
            return _parent->_resolve<IFace>(slc);
        }
        
        auto ptr = nsl->tryResolve(name, slc);
        if (ptr == nullptr) {
            if (_parent == nullptr) {
                throw UnableToResolveException(std::string("Unable to resolve <") + slc->getInterfaceTypeName() + ">  resolve path = " + slc->getResolvePath());
            }
            return _parent->_resolve<IFace>(slc);
        }
        
        return ptr;
    }

    // Resolve a named interface, throws if not able to resolve
    template <class IFace>
    void _visitAll(std::function<void(sptr<typename TypedServiceLocator<IFace>::shared_ptr_binding>)> fnVisit) {
        auto nsl = getTypedServiceLocator<IFace>(false);
        if (nsl != nullptr) {
            nsl->visitAll(fnVisit);
        }
        
        if (_parent != nullptr) {
            _parent->_visitAll<IFace>(fnVisit);
        }
    }

    template <class IFace>
    bool _canResolve(sptr<Context> slc) {
        auto nsl = getTypedServiceLocator<IFace>(false);
        if (nsl == nullptr) {
            if (_parent == nullptr) {
                return false;
            }
            
            return _parent->_canResolve<IFace>(slc);
        }
        
        return nsl->canResolve(slc->getName());
    }
    
    // Try to resolve a named interface, returns nullptr on failure
    template <class IFace>
    sptr<IFace> _tryResolve(sptr<Context> slc) {
        auto nsl = getTypedServiceLocator<IFace>(false);
        if (nsl == nullptr) {
            if (_parent == nullptr) {
                return nullptr;
            }
            
            return _parent->_tryResolve<IFace>(slc);
        }

        auto ptr = nsl->tryResolve(slc->getName(), slc);
        if (ptr == nullptr && _parent != nullptr) {
            return _parent->_tryResolve<IFace>(slc);
        }
        return ptr;
    }

public:
    // Create a root ServiceLocator
    static sptr<ServiceLocator> create() {
        auto slp = sptr<ServiceLocator>(new ServiceLocator());
        
        // Keep a weak reference to ourselves (weird) - have to do this in order to be able to
        // create children which have shared_ptr's to their parents - you cannot create 2 shared_ptr
        // instances from a raw pointer you will crash on 2nd shared_ptr going out of scope and deleting
        // the instance which has already been deleted by the 1st shared_ptr going out of scope
        slp->_this = slp;
        slp->_context = sptr<Context>(new Context(slp));

        return slp;
    }
    
    virtual ~ServiceLocator() {
    }
    
    // Create a child ServiceLocator.  Children can override parent bindings or add new ones (they cannot delete
    // a parent binding).  Children will revert to their parents for unresolved bindings
    sptr<ServiceLocator> enter() {
        auto slp = sptr<ServiceLocator>(new ServiceLocator(sptr<ServiceLocator>(_this)));
        slp->_this = slp;
        slp->_context = sptr<Context>(new Context(slp));
        return slp;
    }
    
    // Create a named binding
    template <class IFace>
    typename TypedServiceLocator<IFace>::shared_ptr_binding::to_clause& bind(const std::string& named) {
        auto nsl = getTypedServiceLocator<IFace>(true);
        
        return nsl->bind(named, &_eagerBindings);
    }
    
    // Create a binding
    template <class IFace>
    typename TypedServiceLocator<IFace>::shared_ptr_binding::to_clause& bind() {
        auto nsl = getTypedServiceLocator<IFace>(true);
        
        return nsl->bind("", &_eagerBindings);
    }
    
    sptr<Context> getContext() const {
        if (_eagerBindings.size() > 0) {
            for(auto eagerBinding : _eagerBindings) {
                eagerBinding->eagerBind(_context);
            }
            _eagerBindings.clear();
        }
        return _context;
    }
    
    
    class module_clause;
    
    // Derive from this to create custom Modules to load from
    class Module {
        friend class module_clause;
        
    protected:
        sptr<ServiceLocator> _sl;
        
    public:
        // Create a named binding
        template <class IFace>
        typename TypedServiceLocator<IFace>::shared_ptr_binding::to_clause& bind(const std::string& named) {
            return _sl->bind<IFace>(named);
        }
    
        // Create a binding
        template <class IFace>
        typename TypedServiceLocator<IFace>::shared_ptr_binding::to_clause& bind() {
            return _sl->bind<IFace>();
        }
        
    public:
        virtual void load() = 0;
    };
    
    
    // Simple module loading syntax
    // modules().add<MyModule>().add<MyOtherModule>()....
    
    class module_clause {
    private:
        mutable sptr<ServiceLocator> _sl;
    
    public:
        module_clause(sptr<ServiceLocator> sl) : _sl(sl) {
        }
        
        template <class TModule>
        module_clause& add() {
            auto module = uptr<TModule>(new TModule());
            module->_sl = _sl;
            
            module->load();
            
            return *this;
        }

        module_clause& add(ServiceLocator::Module&& module) {
            module._sl = _sl;
            
            module.load();
            
            return *this;
        }

        module_clause& add(ServiceLocator::Module& module) {
            module._sl = _sl;
            
            module.load();
            
            return *this;
        }
        
    };
    
    sptr<module_clause> _module_clause;
    module_clause& modules() {
        if (_module_clause == nullptr) {
            _module_clause = sptr<module_clause>(new module_clause(sptr<ServiceLocator>(_this)));
        }
        return *_module_clause;
    }

    
    // This function is used to allow ServiceLocator to "manage" objects which are actually owned outside
    // of ServiceLocator's realm (ie ServiceLocator should not be able to delete the instance through shared_ptr
    // reference decrementing to 0)
    //
    // This is useful when something else owns an object we need to Inject, ServiceLocator always returns
    // shared_ptr<IFace> objects.  In order to make ServiceLocator not delete the alien instance, bind like so
    //
    // bind<IFace>().toInstance(sptr<IFace>(alienObj, ServiceLocator::NoDelete));
    //
    static void NoDelete(const void*) {
    }
};

typedef sptr<ServiceLocator::Context> SLContext_sptr;

#endif /* ServiceLocator_hpp */
