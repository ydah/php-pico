--TEST--
object clone and property COW vector 004
--FILE--
<?php
class GeneratedClone004 { public $items; public function __construct($items) { $this->items = $items; } }
$original = new GeneratedClone004([7, 13]);
$copy = clone $original;
$copy->items[0] = 44;
echo implode(',', $original->items), ':', implode(',', $copy->items);
--EXPECT--
7,13:44,13
