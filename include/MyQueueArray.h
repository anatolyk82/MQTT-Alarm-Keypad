// The source code is taken from: https://github.com/EinarArnason/ArduinoQueue

#ifndef MY_QUEUEARRAY_H
#define MY_QUEUEARRAY_H

// include Arduino basic header.
#include <Arduino.h>

// the definition of the queue class.
template<typename T>
class QueueArray {
  public:
    // init the queue (constructor).
    QueueArray (int size);

    // clear the queue (destructor).
    ~QueueArray ();

    // add an item to the queue.
    void enqueue (const T i);
    
    // remove an item from the queue.
    T dequeue ();

    // get the front of the queue.
    T front () const;

    // empty the queue
    void empty();

    // check if the queue is empty.
    bool isEmpty () const;

    // get the number of items in the queue.
    int count () const;

    // check if the queue is full.
    bool isFull () const;

    // set the printer of the queue.
    void setPrinter (Print & p);

    // remove the last item from the queue
    T removeTail();

    T* content() const;

  private:
    Print * printer; // the printer of the queue.
    T * contents;    // the array of the queue.

    int size;        // the size of the queue.
    int items;       // the number of items of the queue.
};

// init the queue (constructor).
template<typename T>
QueueArray<T>::QueueArray (int _size) {
  size = _size;

  printer = NULL; // set the printer of queue to point nowhere.

  // allocate enough memory for the array.
  contents = (T *) malloc (sizeof (T) * size);
  // if there is a memory allocation error.
  if (contents == NULL) {
    printer->println("QUEUE: insufficient memory to initialize queue.");
  } else {
    empty();
  }
}


// clear the queue (destructor).
template<typename T>
QueueArray<T>::~QueueArray () {
  free (contents); // deallocate the array of the queue.

  contents = NULL; // set queue's array pointer to nowhere.
  printer = NULL;  // set the printer of queue to point nowhere.

  size = 0;        // set the size of queue to zero.
  items = 0;       // set the number of items of queue to zero.
}


// add an item to the queue.
template<typename T>
void QueueArray<T>::enqueue (const T el) {
  if ( isFull() ) {
    for (byte i=1; i<size; i++) {
      contents[(i-1)] = contents[i];
    }
    contents[(items-1)] = el;
    items = size;
  } else {
    contents[items] = el;
    items++;
  }
}


// remove an item from the queue.
template<typename T>
T QueueArray<T>::dequeue () {
  if ( isEmpty() ) {
    if (printer)
      printer->println("QUEUE: can't pop item from queue: queue is empty.");
    return 0;
  }

  T item = contents[0];
  for (byte i=1; i<items; i++) {
    contents[(i-1)] = contents[i];
  }

  items--;

  return item;
}


// get the front of the queue.
template<typename T>
T QueueArray<T>::front () const {
  // check if the queue is empty.
  if (isEmpty ()) {
    if (printer)
      printer->println("QUEUE: can't get the front item of queue: queue is empty.");
    return NULL;
  }
    
  // get the item from the array.
  return contents[0];
}


// remove the last item from the queue
template<typename T>
T QueueArray<T>::removeTail () {
  // check if the queue is empty.
  printer->print("items:");
  printer->println(items);
  
  if ( isEmpty() ) {
    if (printer)
      printer->println("QUEUE: can't remove the tail from the queue: the queue is empty.");
    return 0;
  }

  T item = contents[(items-1)];
  contents[(items-1)] = 0;
  items--;

  return item;
}

template<typename T>
void QueueArray<T>::empty() {
  memset(contents, 0, size);
  items = 0;
}


template<typename T>
T* QueueArray<T>::content () const {
  return contents;
}


// check if the queue is empty.
template<typename T>
bool QueueArray<T>::isEmpty () const {
  return (items == 0);
}


// check if the queue is full.
template<typename T>
bool QueueArray<T>::isFull () const {
  return items == size;
}


// get the number of items in the queue.
template<typename T>
int QueueArray<T>::count () const {
  return items;
}


// set the printer of the queue.
template<typename T>
void QueueArray<T>::setPrinter (Print & p) {
  printer = &p;
}


#endif // MY_QUEUEARRAY_H
