#include "catch.hpp"

#include <dekaf2/web/html/bits/khtmldom_node.h>
#include <dekaf2/containers/memory/karenaallocator.h>
#include <dekaf2/core/strings/kstringview.h>

using namespace dekaf2;
using namespace dekaf2::khtml;

TEST_CASE("khtml::NodePOD")
{
	SECTION("layout")
	{
		// the type is not part of the public API but the size matters for
		// the arena migration: 96 bytes is the design budget. If this
		// assertion fires, the design doc in notes/khtmldom-arena-design.md
		// must be updated alongside any layout change.
		CHECK ( sizeof(NodePOD) <= 96 );
		CHECK ( sizeof(AttrPOD) <= 48 );

		// triviality is also enforced via static_assert in the header,
		// but doubling up here makes a clean test failure if someone
		// accidentally adds a non-trivial member
		CHECK ( std::is_trivially_destructible<NodePOD>::value );
		CHECK ( std::is_trivially_destructible<AttrPOD>::value );
	}

	SECTION("CreateNode initializes a fresh element")
	{
		KArenaAllocator arena;

		NodePOD* n = CreateNode(arena, NodeKind::Element);

		REQUIRE ( n != nullptr );
		CHECK ( n->Kind     == NodeKind::Element );
		CHECK ( n->Flags    == NodeFlag::None );
		CHECK ( n->TagProps == KHTMLObject::TagProperty::None );
		CHECK ( n->Name.empty() );
		CHECK ( n->Value.empty() );
		CHECK ( n->FirstAttr   == nullptr );
		CHECK ( n->LastAttr    == nullptr );
		CHECK ( n->FirstChild  == nullptr );
		CHECK ( n->LastChild   == nullptr );
		CHECK ( n->NextSibling == nullptr );
		CHECK ( n->PrevSibling == nullptr );
		CHECK ( n->Parent      == nullptr );
		CHECK ( CountChildren(n) == 0 );
		CHECK ( CountAttrs(n)    == 0 );
	}

	SECTION("CreateAttr copies name and value into arena")
	{
		KArenaAllocator arena;

		const char* sLiteralName  = "id";
		const char* sLiteralValue = "main";

		AttrPOD* a = CreateAttr(arena, sLiteralName, sLiteralValue);

		REQUIRE ( a != nullptr );
		CHECK ( a->Name  == "id"   );
		CHECK ( a->Value == "main" );
		// the bytes must be arena-owned, not the literal
		CHECK ( a->Name.data()  != sLiteralName  );
		CHECK ( a->Value.data() != sLiteralValue );
		CHECK ( a->Next     == nullptr );
		CHECK ( a->DoEscape == true    );
	}

	SECTION("AppendChild builds a doubly-linked sibling chain")
	{
		KArenaAllocator arena;

		NodePOD* parent = CreateNode(arena, NodeKind::Element);
		NodePOD* a      = CreateNode(arena, NodeKind::Element);
		NodePOD* b      = CreateNode(arena, NodeKind::Element);
		NodePOD* c      = CreateNode(arena, NodeKind::Element);

		AppendChild(parent, a);
		AppendChild(parent, b);
		AppendChild(parent, c);

		CHECK ( CountChildren(parent) == 3 );
		CHECK ( parent->FirstChild == a );
		CHECK ( parent->LastChild  == c );

		// forward chain
		CHECK ( a->NextSibling == b );
		CHECK ( b->NextSibling == c );
		CHECK ( c->NextSibling == nullptr );

		// backward chain
		CHECK ( c->PrevSibling == b );
		CHECK ( b->PrevSibling == a );
		CHECK ( a->PrevSibling == nullptr );

		// parent pointers
		CHECK ( a->Parent == parent );
		CHECK ( b->Parent == parent );
		CHECK ( c->Parent == parent );
	}

	SECTION("AppendAttr builds a singly-linked attr chain")
	{
		KArenaAllocator arena;

		NodePOD* node = CreateNode(arena, NodeKind::Element);

		AppendAttr(node, CreateAttr(arena, "id",    "main"));
		AppendAttr(node, CreateAttr(arena, "class", "wide"));
		AppendAttr(node, CreateAttr(arena, "data",  "x"));

		REQUIRE ( CountAttrs(node) == 3 );

		AttrPOD* a = node->FirstAttr;
		REQUIRE ( a != nullptr );
		CHECK ( a->Name == "id" );
		a = a->Next;
		REQUIRE ( a != nullptr );
		CHECK ( a->Name == "class" );
		a = a->Next;
		REQUIRE ( a != nullptr );
		CHECK ( a->Name == "data" );
		CHECK ( a->Next == nullptr );
		CHECK ( node->LastAttr == a );
	}

	SECTION("FindAttr by name")
	{
		KArenaAllocator arena;

		NodePOD* node = CreateNode(arena, NodeKind::Element);
		AppendAttr(node, CreateAttr(arena, "id",    "x"));
		AppendAttr(node, CreateAttr(arena, "class", "y"));

		AttrPOD* a = FindAttr(node, "class");
		REQUIRE ( a != nullptr );
		CHECK ( a->Value == "y" );

		CHECK ( FindAttr(node, "missing") == nullptr );

		// const overload
		const NodePOD* cnode = node;
		const AttrPOD* ca    = FindAttr(cnode, "id");
		REQUIRE ( ca != nullptr );
		CHECK ( ca->Value == "x" );
	}

	SECTION("InsertChildBefore at head, middle, and end")
	{
		KArenaAllocator arena;

		NodePOD* parent = CreateNode(arena, NodeKind::Element);
		NodePOD* a      = CreateNode(arena, NodeKind::Element);
		NodePOD* b      = CreateNode(arena, NodeKind::Element);

		AppendChild(parent, a);
		AppendChild(parent, b);

		// insert at head: before 'a'
		NodePOD* head = CreateNode(arena, NodeKind::Element);
		InsertChildBefore(parent, a, head);

		CHECK ( parent->FirstChild == head );
		CHECK ( head->NextSibling  == a );
		CHECK ( a->PrevSibling     == head );
		CHECK ( head->PrevSibling  == nullptr );

		// insert in the middle: before 'b'
		NodePOD* mid = CreateNode(arena, NodeKind::Element);
		InsertChildBefore(parent, b, mid);

		CHECK ( a->NextSibling == mid );
		CHECK ( mid->PrevSibling == a );
		CHECK ( mid->NextSibling == b );
		CHECK ( b->PrevSibling   == mid );

		// insert at the end: before nullptr → behaves like AppendChild
		NodePOD* tail = CreateNode(arena, NodeKind::Element);
		InsertChildBefore(parent, nullptr, tail);

		CHECK ( parent->LastChild  == tail );
		CHECK ( b->NextSibling     == tail );
		CHECK ( tail->PrevSibling  == b );

		CHECK ( CountChildren(parent) == 5 );
	}

	SECTION("DetachChild from head, middle, and tail")
	{
		KArenaAllocator arena;

		NodePOD* parent = CreateNode(arena, NodeKind::Element);
		NodePOD* a      = CreateNode(arena, NodeKind::Element);
		NodePOD* b      = CreateNode(arena, NodeKind::Element);
		NodePOD* c      = CreateNode(arena, NodeKind::Element);

		AppendChild(parent, a);
		AppendChild(parent, b);
		AppendChild(parent, c);

		// detach middle
		DetachChild(b);
		CHECK ( CountChildren(parent) == 2 );
		CHECK ( a->NextSibling == c );
		CHECK ( c->PrevSibling == a );
		CHECK ( b->Parent      == nullptr );
		CHECK ( b->NextSibling == nullptr );
		CHECK ( b->PrevSibling == nullptr );

		// detach head
		DetachChild(a);
		CHECK ( CountChildren(parent) == 1 );
		CHECK ( parent->FirstChild == c );
		CHECK ( c->PrevSibling     == nullptr );

		// detach tail (now the only child)
		DetachChild(c);
		CHECK ( CountChildren(parent) == 0 );
		CHECK ( parent->FirstChild == nullptr );
		CHECK ( parent->LastChild  == nullptr );

		// detaching an orphaned node is a no-op
		DetachChild(b);
		CHECK ( b->Parent == nullptr );
	}

	SECTION("text node uses Name as content payload")
	{
		KArenaAllocator arena;

		NodePOD* t = CreateNode(arena, NodeKind::Text);
		t->Name    = arena.AllocateString("hello world");
		t->Flags  |= NodeFlag::TextDoNotEscape;

		CHECK ( t->Kind == NodeKind::Text );
		CHECK ( t->Name == "hello world" );
		CHECK ( (t->Flags & NodeFlag::TextDoNotEscape) == NodeFlag::TextDoNotEscape );
	}

	SECTION("repeated build-and-clear keeps allocator behaviour stable")
	{
		KArenaAllocator arena(2 * 1024);  // small block size to force growth

		for (int round = 0; round < 50; ++round)
		{
			NodePOD* root = CreateNode(arena, NodeKind::Element);
			root->Name = arena.AllocateString("html");

			for (int i = 0; i < 20; ++i)
			{
				NodePOD* child = CreateNode(arena, NodeKind::Element);
				child->Name    = arena.AllocateString("div");
				AppendAttr(child, CreateAttr(arena, "id", "x"));
				AppendChild(root, child);

				NodePOD* text = CreateNode(arena, NodeKind::Text);
				text->Name    = arena.AllocateString("some text content");
				AppendChild(child, text);
			}

			REQUIRE ( CountChildren(root) == 20 );
			arena.clear();
		}

		CHECK ( arena.BlockCount() == 0 );
		CHECK ( arena.UsedBytes()  == 0 );
	}
}
