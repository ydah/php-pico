--TEST--
private and protected member access vector 000
--FILE--
<?php
class GeneratedVisibility000 {
    private $left = 4; protected $right = 6;
    private function left() { return $this->left; }
    protected function right() { return $this->right; }
    public function total() { return $this->left() + $this->right(); }
}
echo (new GeneratedVisibility000())->total();
--EXPECT--
10
