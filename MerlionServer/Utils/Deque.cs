using System;
using System.Runtime.CompilerServices;
using System.Collections.Generic;

namespace Merlion.Server.Utils
{
	sealed class Deque<T>: IList<T>
	{
		T[] array;
		int head = 0;
		int count = 0;
		int ticket = 0;

		public Deque (): this(4)
		{
		}
		public Deque (int capacity)
		{
			if (capacity < 0)
				throw new ArgumentOutOfRangeException ("capacity");
			if (capacity == 0)
				capacity = 1;

			// Make power of two
			--capacity;
			capacity |= capacity >> 1;
			capacity |= capacity >> 2;
			capacity |= capacity >> 4;
			capacity |= capacity >> 8;
			capacity |= capacity >> 16;
			checked {
				++capacity;
			}

			array = new T[capacity];
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		void SetValueUnsafe(int index, T value)
		{
			array [(head + index) & (array.Length - 1)] = value;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		T GetValueUnsafe(int index)
		{
			return array [(head + index) & (array.Length - 1)];
		}

		public T this [int index]
		{
			[MethodImpl(MethodImplOptions.AggressiveInlining)]
			get {
				if (index < 0 || index >= count)
					throw new IndexOutOfRangeException ();
				return GetValueUnsafe (index);
			}
			[MethodImpl(MethodImplOptions.AggressiveInlining)]
			set {
				if (index < 0 || index >= count)
					throw new IndexOutOfRangeException ();
				SetValueUnsafe (index, value);
			}
		}

		public int Count
		{
			get { return count; }
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void EnsureCapacity(int cap)
		{
			if (array.Length >= cap) {
				return;
			}

			int newCount = array.Length;
			while (newCount < cap)
				newCount <<= 1;
			if (newCount <= 0)
				throw new InvalidOperationException ("Too many elements.");

			// Perform resize.
			int oldLength = array.Length;
			int unwrapped = array.Length - head;
			var newArray = new T[newCount];
			if (unwrapped >= count) {
				Array.Copy (array, head, newArray, 0, count);
			} else {
				Array.Copy (array, head, newArray, 0, unwrapped);

				int wrapped = count - unwrapped;
				Array.Copy (array, 0, newArray, unwrapped, wrapped);
			}
			head = 0;
			array = newArray;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void AddLast(T newItem)
		{
			EnsureCapacity (count + 1);
			array[(head + count) & (array.Length - 1)] = newItem;
			++count;
			++ticket;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void AddFirst(T newItem)
		{
			EnsureCapacity (count + 1);
			++count;
			head = (head - 1) & (array.Length - 1);
			array[head] = newItem;
			++ticket;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void RemoveFirst()
		{
			if (count == 0)
				throw new InvalidOperationException ("Collection is empty.");
			--count;
			head = (head + 1) & (array.Length - 1);
			++ticket;
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public void RemoveLast()
		{
			if (count == 0)
				throw new InvalidOperationException ("Collection is empty.");
			--count;
			++ticket;
		}

		void ICollection<T>.Add (T item)
		{
			AddLast (item);
		}
		public void Clear ()
		{
			count = 0;
			++ticket;
		}
		public bool Contains (T item)
		{
			var eq = EqualityComparer<T>.Default;
			foreach (var i in this)
				if (eq.Equals (i, item))
					return true;
			return false;
		}
		public void CopyTo (T[] array, int arrayIndex)
		{
			if (array == null)
				throw new ArgumentNullException ("array");
			if (Count + arrayIndex > array.Length)
				throw new IndexOutOfRangeException ();
			foreach (var i in this)
				array [arrayIndex++] = i;
		}
		public bool Remove (T item)
		{
			throw new NotImplementedException ();
		}
		public bool IsReadOnly {
			get {
				return false;
			}
		}
		public IEnumerator<T> GetEnumerator ()
		{
			int ticket = this.ticket;
			int head = this.head, mask = this.count - 1;
			var array = this.array;
			while (count > 0) {
				if (ticket != this.ticket) {
					throw new InvalidOperationException ("Collection was modified.");
				}
				yield return array[head];
				head = (head + 1) & mask;
				--count;
			}
		}
		System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator ()
		{
			foreach (var i in this)
				yield return i;
		}

		public int IndexOf (T item)
		{
			var eq = EqualityComparer<T>.Default;
			int idx = 0;
			foreach (var i in this) {
				if (eq.Equals (i, item))
					return idx;
				++idx;
			}
			return -1;
		}
		public void Insert (int index, T item)
		{
			throw new NotImplementedException ();
		}
		public void RemoveAt (int index)
		{
			throw new NotImplementedException ();
		}
	}
}

