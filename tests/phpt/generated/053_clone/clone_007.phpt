--TEST--
object clone and property COW vector 007
--FILE--
<?php
class GeneratedClone007 { public $items; public function __construct($items) { $this->items = $items; } }
$original = new GeneratedClone007([10, 19]);
$copy = clone $original;
$copy->items[0] = 71;
echo implode(',', $original->items), ':', implode(',', $copy->items);
--EXPECT--
10,19:71,19
