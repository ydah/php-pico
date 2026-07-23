--TEST--
object clone and property COW vector 003
--FILE--
<?php
class GeneratedClone003 { public $items; public function __construct($items) { $this->items = $items; } }
$original = new GeneratedClone003([6, 11]);
$copy = clone $original;
$copy->items[0] = 35;
echo implode(',', $original->items), ':', implode(',', $copy->items);
--EXPECT--
6,11:35,11
