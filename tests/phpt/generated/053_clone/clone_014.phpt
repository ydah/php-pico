--TEST--
object clone and property COW vector 014
--FILE--
<?php
class GeneratedClone014 { public $items; public function __construct($items) { $this->items = $items; } }
$original = new GeneratedClone014([17, 33]);
$copy = clone $original;
$copy->items[0] = 134;
echo implode(',', $original->items), ':', implode(',', $copy->items);
--EXPECT--
17,33:134,33
