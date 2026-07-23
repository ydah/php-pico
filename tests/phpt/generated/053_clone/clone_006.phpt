--TEST--
object clone and property COW vector 006
--FILE--
<?php
class GeneratedClone006 { public $items; public function __construct($items) { $this->items = $items; } }
$original = new GeneratedClone006([9, 17]);
$copy = clone $original;
$copy->items[0] = 62;
echo implode(',', $original->items), ':', implode(',', $copy->items);
--EXPECT--
9,17:62,17
