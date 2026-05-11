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
		CHECK ( sizeof(AttrPOD) <= 56 );

		// triviality is also enforced via static_assert in the header,
		// but doubling up here makes a clean test failure if someone
		// accidentally adds a non-trivial member
		CHECK ( std::is_trivially_destructible<NodePOD>::value );
		CHECK ( std::is_trivially_destructible<AttrPOD>::value );
	}

	SECTION("AddNode initializes a fresh element")
	{
		Document doc;

		NodePOD* n = doc.AddNode(NodeKind::Element);

		REQUIRE ( n != nullptr );
		CHECK ( n->Kind()          == NodeKind::Element );
		CHECK ( n->Flags()         == NodeFlag::None );
		CHECK ( n->TagProps()      == KHTMLObject::TagProperty::None );
		CHECK ( n->Name ().empty() );
		CHECK ( n->Value().empty() );
		CHECK ( n->FirstAttr()     == nullptr );
		CHECK ( n->LastAttr()      == nullptr );
		CHECK ( n->FirstChild()    == nullptr );
		CHECK ( n->LastChild()     == nullptr );
		CHECK ( n->NextSibling()   == nullptr );
		CHECK ( n->PrevSibling()   == nullptr );
		CHECK ( n->Parent()        != nullptr );
		CHECK ( n->CountChildren() == 0 );
		CHECK ( n->CountAttrs()    == 0 );
	}

	SECTION("AddAttribute keeps name and value outside arena")
	{
		Document doc;

		const char* sLiteralName  = "id";
		const char* sLiteralValue = "main";

		auto* root = doc.AddNode(NodeKind::Element);
		AttrPOD* a = root->AddAttribute(sLiteralName, sLiteralValue);

		REQUIRE ( a != nullptr );
		CHECK ( a->Name () == "id"   );
		CHECK ( a->Value() == "main" );
		// the bytes should still point to the literals, as they are const in the data segment
		CHECK ( a->Name().data()  == sLiteralName  );
		CHECK ( a->Value().data() == sLiteralValue );
		CHECK ( a->Next()     == nullptr );
		CHECK ( a->DoEscape() == true    );
	}

	SECTION("AddAttribute copies name and value into arena")
	{
		Document doc;

		KString sLiteralName  = "id";
		KString sLiteralValue = "main";

		auto* root = doc.AddNode(NodeKind::Element);
		AttrPOD* a = root->AddAttribute(sLiteralName, sLiteralValue);

		REQUIRE ( a != nullptr );
		CHECK ( a->Name () == "id"   );
		CHECK ( a->Value() == "main" );
		// the bytes must now be arena-owned
		CHECK ( a->Name().data()  != sLiteralName.data()  );
		CHECK ( a->Value().data() != sLiteralValue.data() );
		CHECK ( a->Next()     == nullptr );
		CHECK ( a->DoEscape() == true    );
	}

	SECTION("AddNode builds a doubly-linked sibling chain")
	{
		Document doc;

		NodePOD* parent = doc.AddNode(NodeKind::Element);
		NodePOD* a      = parent->AddNode(NodeKind::Element);
		NodePOD* b      = parent->AddNode(NodeKind::Element);
		NodePOD* c      = parent->AddNode(NodeKind::Element);

		CHECK ( parent->CountChildren() == 3 );
		CHECK ( parent->FirstChild() == a );
		CHECK ( parent->LastChild()  == c );

		// forward chain
		CHECK ( a->NextSibling() == b );
		CHECK ( b->NextSibling() == c );
		CHECK ( c->NextSibling() == nullptr );

		// backward chain
		CHECK ( c->PrevSibling() == b );
		CHECK ( b->PrevSibling() == a );
		CHECK ( a->PrevSibling() == nullptr );

		// parent pointers
		CHECK ( a->Parent() == parent );
		CHECK ( b->Parent() == parent );
		CHECK ( c->Parent() == parent );
	}

	SECTION("AddAttribute builds a singly-linked attr chain")
	{
		Document doc;

		NodePOD* node = doc.AddNode(NodeKind::Element);

		node->AddAttribute("id", "main");
		node->AddAttribute("class", "wide");
		node->AddAttribute("data",  "x");

		REQUIRE ( node->CountAttrs() == 3 );

		AttrPOD* a = node->FirstAttr();
		REQUIRE ( a != nullptr );
		CHECK ( a->Name() == "id" );
		a = a->Next();
		REQUIRE ( a != nullptr );
		CHECK ( a->Name() == "class" );
		a = a->Next();
		REQUIRE ( a != nullptr );
		CHECK ( a->Name() == "data" );
		CHECK ( a->Next() == nullptr );
		CHECK ( node->LastAttr() == a );
	}

	SECTION("FindAttr by name")
	{
		Document doc;

		NodePOD* node = doc.AddNode(NodeKind::Element);
		node->AddAttribute("id",    "x");
		node->AddAttribute("class", "y");

		AttrPOD* a = node->Attribute("class");
		REQUIRE ( a != nullptr );
		CHECK ( a->Value() == "y" );

		CHECK ( node->Attribute("missing") == nullptr );

		// const overload
		const NodePOD* cnode = node;
		const AttrPOD* ca    = cnode->Attribute("id");
		REQUIRE ( ca != nullptr );
		CHECK ( ca->Value() == "x" );
	}

	SECTION("InsertChildBefore at head, middle, and end")
	{
		Document doc;

		NodePOD* parent = doc.AddNode(NodeKind::Element);
		NodePOD* a      = parent->AddNode(NodeKind::Element);
		NodePOD* b      = parent->AddNode(NodeKind::Element);

		// insert at head: before 'a'
		NodePOD* head = parent->AddNode(a, NodeKind::Element);

		CHECK ( parent->FirstChild() == head );
		CHECK ( head->NextSibling()  == a );
		CHECK ( a->PrevSibling()     == head );
		CHECK ( head->PrevSibling()  == nullptr );

		// insert in the middle: before 'b'
		NodePOD* mid = parent->AddNode(b, NodeKind::Element);

		CHECK ( a->NextSibling()   == mid );
		CHECK ( mid->PrevSibling() == a );
		CHECK ( mid->NextSibling() == b );
		CHECK ( b->PrevSibling()   == mid );

		// insert at the end: before nullptr → behaves like AppendChild
		NodePOD* tail = parent->AddNode(nullptr, NodeKind::Element);

		CHECK ( parent->LastChild()  == tail );
		CHECK ( b->NextSibling()     == tail );
		CHECK ( tail->PrevSibling()  == b );

		CHECK ( parent->CountChildren() == 5 );
	}

	SECTION("Detach from head, middle, and tail")
	{
		Document doc;

		NodePOD* parent = doc.AddNode(NodeKind::Element);
		NodePOD* a      = parent->AddNode(NodeKind::Element);
		NodePOD* b      = parent->AddNode(NodeKind::Element);
		NodePOD* c      = parent->AddNode(NodeKind::Element);

		// detach middle
		b->Detach();
		CHECK ( parent->CountChildren() == 2 );
		CHECK ( a->NextSibling() == c );
		CHECK ( c->PrevSibling() == a );
		CHECK ( b->Parent()      == nullptr );
		CHECK ( b->NextSibling() == nullptr );
		CHECK ( b->PrevSibling() == nullptr );

		// detach head
		a->Detach();
		CHECK ( parent->CountChildren() == 1 );
		CHECK ( parent->FirstChild() == c );
		CHECK ( c->PrevSibling()     == nullptr );

		// detach tail (now the only child)
		c->Detach();
		CHECK ( parent->CountChildren() == 0 );
		CHECK ( parent->FirstChild() == nullptr );
		CHECK ( parent->LastChild()  == nullptr );

		// detaching an orphaned node is a no-op
		b->Detach();
		CHECK ( b->Parent() == nullptr );
	}

	SECTION("text node uses Name as content payload")
	{
		Document doc;

		NodePOD* t = doc.AddNode(NodeKind::Text);
		t->Name("hello world");
		t->Flags(t->Flags() | NodeFlag::TextDoNotEscape);

		CHECK ( t->Kind() == NodeKind::Text );
		CHECK ( t->Name() == "hello world" );
		CHECK ( (t->Flags() & NodeFlag::TextDoNotEscape) == NodeFlag::TextDoNotEscape );
	}

	SECTION("repeated build-and-clear keeps allocator behaviour stable")
	{
		Document doc(2 * 1024);  // small block size to force growth

		for (int round = 0; round < 50; ++round)
		{
			NodePOD* root = doc.AddNode(NodeKind::Element);
			root->Name("html");

			for (int i = 0; i < 20; ++i)
			{
				NodePOD* child = root->AddNode(NodeKind::Element);
				child->Name("div");
				child->AddAttribute("id", "x");

				NodePOD* text = child->AddNode(NodeKind::Text);
				text->Name("some text content");
			}

			REQUIRE ( root->CountChildren() == 20 );
			doc.clear();
		}

		CHECK ( doc.BlockCount() == 0 );
		CHECK ( doc.UsedBytes()  == 0 );
	}
}
