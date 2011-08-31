// find the ancestor matching this tag
function findAncestor (o, tag)
{
  while (o && (o.nodeName != tag))
	{
    if (o.parentNode)
		  o = o.parentNode;
  	else if (o.offsetParent)
	    o = o.offsetParent;
	}
	return (o);
}

// find the previous sibling matching this tag
function findPrevSibling (o, tag)
{
  while (o && (o.nodeName != tag))
	  o = o.previousSibling;
	return (o);
}

// find the next sibling matching this tag
function findNextSibling (o, tag)
{
  while (o && (o.nodeName != tag))
	  o = o.nextSibling;
	return (o);
}

// find the child matching this tag
function findChild (o, tag)
{
  if (o)
   o = findNextSibling (o.firstChild, tag);
	return (o);
}

// add a field to a queue type

function addfield (o) {
  o = findAncestor (o, "TR");
	// get the index of the previous row
	var p = findPrevSibling (o.previousSibling, "TR");
	p = findChild (p, "TD");
	var i = p.innerHTML.replace (/^\s*Field/, "") - 0;
  // create a new table row
	var c = document.createElement("TR");
	var d = document.createElement ("TD");
	// add the prompt
	d.appendChild (document.createTextNode ("Field " + (i + 1)));
	c.appendChild (d);
  // get input entry
	p = findChild (findNextSibling (p.nextSibling, "TD"), "INPUT");
	var f = document.createElement ("INPUT");
	f.type = p.type;
	f.size = p.size;
	f.name = p.name.replace (/Field.*\]/, "Field[" + i + "]");
	d = document.createElement ("TD");
	d.appendChild (f);
	c.appendChild (d);
  // finally place it in the form
  if (o.parentNode)
	  o.parentNode.insertBefore (c, o);
	else
	  o.offsetParent.insertBefore (c, o);
}
