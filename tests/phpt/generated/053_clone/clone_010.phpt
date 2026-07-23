--TEST--
object clone and property COW vector 010
--FILE--
<?php
class GeneratedClone010 { public $items; public function __construct($items) { $this->items = $items; } }
$original = new GeneratedClone010([13, 25]);
$copy = clone $original;
$copy->items[0] = 98;
echo implode(',', $original->items), ':', implode(',', $copy->items);
--EXPECT--
13,25:98,25
