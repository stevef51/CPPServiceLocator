# CPPServiceLocator
A header only C++11 Dependency Injector

Inspired by .NET NInject but by no means anywhere near as sophisticated.

Supports basic binding to self, interface to implementation class, named bindings, function bindings, instance bindings, singletons, eager bindings, aliases and modules.

# Usage
Basics, define interfaces (abstract classes) which client code will use

```c++
class IFoo {
public:
    virtual void fooIt() = 0;
};
```

have other code declare dependency in their constructors 

```c++
class Bar {
private:
   std::shared_ptr<IFoo> _foo;
public:
   Bar(std::shared_ptr<IFoo> foo) : _foo(foo) {
   }
   void doFoo() {
    _foo->fooIt();
   }
};
```

declare implementation classes

```c++
class RedFoo : public IFoo {
public:
  void fooIt() override {
    std::cout << "RedFoo!\n";
  }
};
```

declare the bindings in SLModules

```c++
// RedFooSLModule is intimate with RedFoo, it knows what dependencies RedFoo has (in this case none)
class RedFooSLModule : public ServiceLocator::Module {
public:
  void load() override {
    bind<IFoo>().to<RedFoo>([] (SLContext_sptr slc) {
      return new RedFoo();
    });
  }
};

// BarSLModule is intimate with Bar, it knows what dependencies Bar has ..
class BarSLModule : public ServiceLocator::Module {
public:
  void load() override {
    bind<Bar>().toSelf([] (SLContext_ptr slc) {
      return new Bar(
        slc->resolve<IFoo>()
      );    
    })
  }
};
```

load the modules at startup (use configuration to choose which modules are loaded = nice)

```c++
auto sl = ServiceLocator::create();
sl->modules().add<RedFooSLModule>().add<BarSLModule>();
```

and request your root object(s) 

```c++
auto slc = sl->getContext();
auto bar = slc->resolve<Bar>();
```

# Why is it called ServiceLocator but you said it does Dependency Injection?
The ServiceLocator class does not do Dependency Injection on its own, which is why I chose not to call it a DependencyInjector - the Dependency Injection occurs by how you code your bindings.  Using the lambda function bindings to return "new" instances is where the Dependency Injection occurs. It's not Reflection, but it works really well (see above, examples/example_dependency_injector and tests/)

# Aliases
Each bind only allows 1 interface to 1 implementation.  Use aliases to bind multiple interfaces to 1 implementation :-

```c++
bind<Foo>().toSelf([] (SLContext_sptr) { return new Foo(); });
bind<IFoo>().alias<Foo>();
bind<IFoo2>().alias<Foo>();
```

# Singleton or Transient
Currently only Transient (default) (new instance on every resolve) and Singleton (same instance globally) are supported.

```c++
bind<IFoo>().to<Foo>([] (SLContext_sptr slc) { return new Foo(); }).asSingleton();
```

# Named bindings
Binding an un-named interface more than once will (within any given ServiceLocator) will throw a DuplicateBindingException, named bindings allow multiples 

```c++
bind<IFoo>("RedFoo").to<RedFoo>([] (SLContext_sptr slc) { return RedFoo(); });
bind<IFoo>("BlueFoo").to<BlueFoo>([] (SLContext_sptr slc) { return BlueFoo(); });
```

Given above, both bindings can be resolved with resolveAll

```c++
std::vector<sptr<IFoo>> foos;
slc->resolveAll<IFoo>(&foos);
```

or individually given their name

```c++
auto redFoo = slc->resolve<IFoo>("RedFoo");
auto blueFoo = slc->resolve<IFoo>("BlueFoo");
```

# Child ServiceLocators
A root level ServiceLocator is created using

```c++
auto parent = ServiceLocator::create();
```

child ServiceLocators can be created (deeply nested if needed)

```c++
auto child = parent->enter();
```

bindings within a child do not affect its parent(s), in fact child bindings can override their parent bindings.

```c++
parent->bind<IBar>().toNoDependency<GreenBar>();
parent->bind<IFoo>().toNoDependency<RedFoo>();
child->bind<IFoo>().toNoDependency<BlueFoo>();
```

A child will attempt to resolve within itself first and walk its parent chain until a binding is found before erroring if binding is not found

```c++
auto will_be_BlueFoo = child->resolve<IFoo>();
auto will_be_RedFoo = parent->resolve<IFoo>();
auto will_be_GreenBar = child->resolve<IBar>();
```

# Circular dependency detection

It will automatically detect circular dependency between bindings, eg

```c++
class Foo : public IFoo {
private:
  sptr<IBar> _bar;
public:
  Foo(sptr<IBar> bar) : _bar(bar) {
  }
};

class Bar : public IBar {
private:
  sptr<IFoo> _foo;
public:
  Bar(sptr<IFoo> foo) : _foo(foo) {
  }
};

bind<IFoo>().to<Foo>([] (SLContext_sptr slc) { return new Foo(slc->resolve<IBar>()); }).asSingleton();
bind<IBar>().to<Bar>([] (SLContext_sptr slc) { return new Bar(slc->resolve<IFoo>()); }).asSingleton();

auto bar = slc->resolve<IBar>();      // will throw CircularDependencyException with a message showing the dependency path
```

# Working around Circular dependency - Property injection

It is ofcourse better to design your system such that circular dependencies do not occur, but I have found this is sometimes harder than the alternative which is to use Property Injection.  Property Injection resolves the issue since you make 1 of your classes not take its dependency through construction, allowing it to be instantiated and injected into the other class dependant classes constructor, on completion Property Injection resolves the first classes dependency :-

```c++
class Foo : public IFoo {
private:
  sptr<IBar> _bar;
public:
  void setBar(sptr<IBar> bar) { _bar = bar; }
};

bind<IFoo>().to<Foo>([] (SLContext_sptr slc) {
  auto foo = new Foo();
  slc->afterResolve([foo] (SLContext_sptr slc) {
    foo->setBar(slc->resolve<IBar>());
  });
  return foo;
}).asSingleton();
```

The *afterResolve* method queues a call to the lambda after the root object has been instantiated, avoiding the circular dependency.

Ofcourse the other problem created here is that we now have 2 objects Foo and Bar which hold a shared_ptr reference to each other, even when the ServiceLocator is released (at the end of the program) these 2 objects will continue their lives.  In this case one of them would need to explicitly release the other 

```c++
void onExit() {
    foo->setBar(nullptr);
}
```

# sptr -> std::shared_ptr
At the moment ServiceLocator uses std::shared_ptr to handle instance life times, Singletons are held in memory via a cached std::shared_ptr and all instances are resolved to std::shared_ptr<IFace>

Although untested, it is possible to use a different *shared_ptr* implementation by defining

```c++
#define SERVICELOCATOR_SPTR
template <class T>
using sptr = boost::shared_ptr<T>;

template <class T>
using const_sptr = boost::shared_ptr<const T>;

template <class T>
using wptr = boost::weak_ptr<T>;

template <class T>
using uptr = boost::unique_ptr<T>;
```

before including "ServiceLocator.hpp"

# Using externally allocated instances
It is possible to have ServiceLocator bind an externally allocated instance using the *NoDelete* deallocation method.  This allows these instances lifetime to be controlled externally whilst still allowing them to be ServiceLocator injected.

```c++
Poco::AutoPtr<Foo> foo = GetFoo();
sl->bind<IFoo>().toInstance(foo.get(), ServiceLocator::NoDelete);

auto foo = slc->resolve<IFoo>();
```

internally the instance is managed using a std::shared_ptr (*sptr*) but will call the *NoDelete* method (which does nothing) when the shared_ptr reference count reaches 0 allowing the Poco::AutoPtr to continue lifetime management.

Ofcourse when doing this you need to gaurantee that the externally allocated instance does not release the instance during the lifetime of the ServiceLocator.



# Why another Dependency Injection library
Firstly, there are not that many for C++ in general.  There are amongst a couple of others, Google Fruit and Boost DI.  Boost DI requires C++14 so I did not even look at this (my project is strictly C++11 limited) and Google Fruit I frankly found too hard to understand how to use - sure, it's almost definitely me, but I am quite familiar with .NET Ninject and was struggling to map concepts to Google Fruit within my deadline.  

Also note, that Google Fruit contaminates your classes with INJECT() macros - not a massive deal but it makes you classes Fruit aware when perhaps they shouldn't be.  Outside of using std::shared_ptr my ServiceLocator requires no such contamination of your interfaces or implementation classes.

So I rolled my own - initially just developed a simple Service Locator - a dictionary of typeids to templated classes - and yes Service Locator is an anti-pattern.  I was prepared to wear the anti-pattern drawbacks initially.  However after getting this going it became ever more apparent this would be hard work for unit tests and development in general since missing dependencies were not caught until runtime.  

Issue (1) is getting C++ to do any sort of constructor injection.  Reflection is not available in C++ which is how NInject does it's magic.
Issue (2) is my classes were ServiceLocator contaminated - they all took a single constructor argument of ServiceLocator* to which they would find their dependencies, this was also bad.

eg

```c++
#include "ServiceLocator.hpp"       // Yikes, ServiceLocator contamination
class Bar {
private:
  std::shared_ptr<IFoo> _foo;
public:
  // CONTAMINATION - My constructor takes a ServiceLocatorContext*, AND my dependencies are hidden until run-time - ouch
  Bar(SLContext_sptr slc) : _foo(slc->resolve<IFoo>()) {
  }
};
```

The anti-pattern is the cause of both issues, and the solution to both is quite simple :-

a) Move the constructor initialisation into a ServiceLocator Module and declare your constructors with explicit dependencies, this means they are not ServiceLocator contaminated (solves (2)) and now your ServiceLocator Modules are the only place that is ServiceLocator aware and you solve issue (1) since constructors are declaring all dependencies - you get compile errors instead of runtime nullptr's.

there is no b)

